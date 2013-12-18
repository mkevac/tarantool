/*
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "request.h"
#include "txn.h"
#include "tuple.h"
#include "index.h"
#include "space.h"
#include "schema.h"
#include "port.h"
#include "lua/call.h"
#include <errinj.h>
#include <pickle.h>
#include <fiber.h>
#include <scoped_guard.h>
#include <third_party/base64.h>

STRS(requests, REQUESTS);

static const char *
read_tuple(const char **reqpos, const char *reqend)
{
	const char *tuple = *reqpos;
	if (unlikely(!mp_check(reqpos, reqend))) {
		say_error("\n"
			  "********************************************\n"
			  "* Found a corrupted tuple in a request!    *\n"
			  "* This can be either due to a memory       *\n"
			  "* corruption or a bug in the server.       *\n"
			  "* The tuple can not be loaded.             *\n"
			  "********************************************\n"
			  "Request tail, BASE64 encoded:               \n");

		uint32_t tuple_len = reqend - tuple;
		int base64_buflen = base64_bufsize(tuple_len);
		char *base64_buf = (char *) malloc(base64_buflen);
		int len = base64_encode(tuple, tuple_len,
					base64_buf, base64_buflen);
		write(STDERR_FILENO, base64_buf, len);
		free(base64_buf);
		tnt_raise(ClientError, ER_INVALID_MSGPACK);
	}

	if (unlikely(mp_typeof(*tuple) != MP_ARRAY))
		tnt_raise(ClientError, ER_TUPLE_NOT_ARRAY);

	return tuple;
}

enum dup_replace_mode
dup_replace_mode(uint32_t type)
{
	switch (type) {
	case INSERT: return DUP_INSERT;
	case REPLACE: return DUP_REPLACE;
	case STORE: return DUP_REPLACE_OR_INSERT;
	default: assert(0);
	}
}

static void
execute_replace(const struct request *request, struct txn *txn,
		struct port *port)
{
	(void) port;
	txn_add_redo(txn, request->req.request_type,
	             request->req.data,
	             request->req.len);

	struct space *space = space_find(request->req.space);
	const char *tuple = request->req.tuples; /* XXX */
	struct tuple *new_tuple =
		tuple_new(space->format, &tuple, request->req.tuples_end);
	TupleGuard guard(new_tuple);
	space_validate_tuple(space, new_tuple);
	enum dup_replace_mode mode =
		dup_replace_mode(request->req.request_type);
	txn_replace(txn, space, NULL, new_tuple, mode);
}

static void
execute_update(const struct request *request, struct txn *txn,
	       struct port *port)
{
	(void) port;
	txn_add_redo(txn, request->req.request_type, request->req.data,
	             request->req.len);
	/* Parse UPDATE request. */
	/** Search key  and key part count. */

	struct space *space = space_find(request->req.space);
	Index *pk = index_find(space, 0);
	/* Try to find the tuple by primary key. */
	const char *key = request->req.keys; /* XXX */
	uint32_t part_count = mp_decode_array(&key);
	primary_key_validate(pk->key_def, key, part_count);
	struct tuple *old_tuple = pk->findByKey(key, part_count);

	if (old_tuple == NULL)
		return;

	/* Update the tuple. */
	struct tuple *new_tuple =
		tuple_update(space->format,
		             region_alloc_cb,
	                 &fiber->gc,
	                 old_tuple, request->req.exprs,
	                 request->req.exprs_end);
	TupleGuard guard(new_tuple);
	space_validate_tuple(space, new_tuple);
	txn_replace(txn, space, old_tuple, new_tuple, DUP_INSERT);
}

/** }}} */

static void
execute_select(const struct request *request, struct txn *txn,
	       struct port *port)
{
	(void) txn;
	struct space *space = space_find(request->req.space);
	Index *index = index_find(space, request->req.index);

	const char *keys = request->req.keys;
	const char *keyp = keys;
	uint32_t key_count = mp_decode_array(&keyp);
	if (key_count == 0)
		tnt_raise(IllegalParams, "tuple count must be positive");

	ERROR_INJECT_EXCEPTION(ERRINJ_TESTING);

	uint32_t found = 0;
	uint32_t offset = request->req.offset;
	uint32_t limit = request->req.limit;
	for (uint32_t i = 0; i < key_count; i++) {

		/* End the loop if reached the limit. */
		if (limit == found)
			return;

		/* read key */
		const char *key = read_tuple(&keys, request->req.keys_end);
		uint32_t part_count = mp_decode_array(&key);

		struct iterator *it = index->position();
		key_validate(index->key_def, ITER_EQ, key, part_count);
		index->initIterator(it, ITER_EQ, key, part_count);

		struct tuple *tuple;
		while ((tuple = it->next(it)) != NULL) {
			if (offset > 0) {
				offset--;
				continue;
			}

			port_add_tuple(port, tuple, BOX_RETURN_TUPLE);

			if (limit == ++found)
				break;
		}
	}

	if (keys != request->req.keys_end)
		tnt_raise(IllegalParams, "can't unpack request");
}

static void
execute_delete(const struct request *request, struct txn *txn,
	       struct port *port)
{
	(void) port;
	txn_add_redo(txn, request->req.request_type,
	             request->req.data,
				 request->req.len);
	struct space *space = space_find(request->req.space);

	/* Try to find tuple by primary key */
	Index *pk = index_find(space, 0);
	const char *key = request->req.keys; /* XXX */
	uint32_t part_count = mp_decode_array(&key);
	primary_key_validate(pk->key_def, key, part_count);
	struct tuple *old_tuple = pk->findByKey(key, part_count);

	if (old_tuple == NULL)
		return;

	txn_replace(txn, space, old_tuple, NULL, DUP_REPLACE_OR_INSERT);
}

/** To collects stats, we need a valid request type.
 * We must collect stats before execute.
 * Check request type here for now.
 */
static bool
request_check_type(uint32_t type)
{
#if 0
	return (type != REPLACE && type != SELECT &&
		type != UPDATE && type != DELETE_1_3 &&
		type != DELETE && type != CALL);
#endif
	switch (type) {
	case REPLACE:
	case INSERT:
	case STORE:
	case DELETE:
	case UPDATE:
	case SELECT:
	case CALL:
		return true;
	}
	return false;
}

const char *
request_name(uint32_t type)
{
	if (request_check_type(type))
		return "unsupported";
	return requests_strs[type];
}

void
request_create(struct request *r, uint32_t type, const char *data,
               uint32_t len)
{
	if (request_check_type(type)) {
		say_error("Unsupported request = %" PRIi32 "", type);
		tnt_raise(IllegalParams, "unsupported command code, "
			  "check the error log");
	}
	memset(r, 0, sizeof(*r));

	int rc = tb_decode_body(&r->req, type, len, data);
	if (rc == -1)
		tnt_raise(IllegalParams, "can't unpack request");

	if (! tb_has(&r->req, TB_OFFSET))
		r->req.offset = 0;
	if (! tb_has(&r->req, TB_LIMIT))
		r->req.limit = UINT32_MAX;
	if (! tb_has(&r->req, TB_INDEX))
		r->req.index = 0;
	if (! tb_has(&r->req, TB_ITERATOR))
		r->req.iterator = ITER_EQ;

	uint64_t mandatory = 0;
	switch (type) {
	case INSERT:
	case STORE:
	case REPLACE:
		mandatory = tb_bit(TB_SPACE)|tb_bit(TB_TUPLES);
		r->execute = execute_replace;
		break;
	case SELECT:
		mandatory = tb_bit(TB_SPACE)|tb_bit(TB_KEYS);
		r->execute = execute_select;
		break;
	case UPDATE:
		mandatory = tb_bit(TB_SPACE)|tb_bit(TB_KEYS) |
		            tb_bit(TB_EXPRS);
		r->execute = execute_update;
		break;
	case DELETE:
		mandatory = tb_bit(TB_SPACE)|tb_bit(TB_KEYS);
		r->execute = execute_delete;
		break;
	case CALL:
		mandatory = tb_bit(TB_TUPLES)|tb_bit(TB_PROCNAME);
		r->execute = box_lua_call;
		break;
	default:
		assert(false);
		break;
	}

	if ((r->req.bitmap & mandatory) == 0)
		tnt_raise(IllegalParams, "insufficient arguments");

#if 0
	memset(request, 0, sizeof(*request));
	request->type = type;
	request->data = data;
	request->len = len;
	request->flags = 0;

	const char **reqpos = &data;
	const char *reqend = data + len;
	const char *s;

	switch (request->type) {
	case REPLACE:
		request->execute = execute_replace;
		request->r.space_no = pick_u32(reqpos, reqend);
		request->flags |= (pick_u32(reqpos, reqend) &
				   BOX_ALLOWED_REQUEST_FLAGS);
		request->r.tuple = read_tuple(reqpos, reqend);
		if (unlikely(*reqpos != reqend))
			tnt_raise(IllegalParams, "can't unpack request");

		request->r.tuple_end = *reqpos;
		break;
	case SELECT:
		request->execute = execute_select;
		request->s.space_no = pick_u32(reqpos, reqend);
		request->s.index_no = pick_u32(reqpos, reqend);
		request->s.offset = pick_u32(reqpos, reqend);
		request->s.limit = pick_u32(reqpos, reqend);
		request->s.key_count = pick_u32(reqpos, reqend);
		request->s.keys = *reqpos;
		/* Do not parse the tail, execute_select will do it */
		request->s.keys_end = reqend;
		break;
	case UPDATE:
		request->execute = execute_update;
		request->u.space_no = pick_u32(reqpos, reqend);
		request->flags |= (pick_u32(reqpos, reqend) &
				   BOX_ALLOWED_REQUEST_FLAGS);
		request->u.key = read_tuple(reqpos, reqend);
		request->u.key_end = *reqpos;
		request->u.expr = *reqpos;
		/* Do not parse the tail, tuple_update will do it */
		request->u.expr_end = reqend;
		break;
	case DELETE_1_3:
	case DELETE:
		request->execute = execute_delete;
		request->d.space_no = pick_u32(reqpos, reqend);
		if (type == DELETE) {
			request->flags |= pick_u32(reqpos, reqend) &
				BOX_ALLOWED_REQUEST_FLAGS;
		}
		request->d.key = read_tuple(reqpos, reqend);
		request->d.key_end = *reqpos;
		if (unlikely(*reqpos != reqend))
			tnt_raise(IllegalParams, "can't unpack request");
		break;
	case CALL:
		request->execute = box_lua_call;
		request->flags |= (pick_u32(reqpos, reqend) &
				   BOX_ALLOWED_REQUEST_FLAGS);
		s = *reqpos;
		if (unlikely(!mp_check(reqpos, reqend)))
			tnt_raise(ClientError, ER_INVALID_MSGPACK);
		if (unlikely(mp_typeof(*s) != MP_STR))
			tnt_raise(ClientError, ER_ARG_TYPE, 0, "STR");
		request->c.procname = mp_decode_str(&s, &request->c.procname_len);
		assert(s == *reqpos);
		request->c.args = read_tuple(reqpos, reqend);
		request->c.args_end = *reqpos;
		if (unlikely(*reqpos != reqend))
			tnt_raise(IllegalParams, "can't unpack request");
		break;
	default:
		assert(false);
		break;
	}
#endif
}
