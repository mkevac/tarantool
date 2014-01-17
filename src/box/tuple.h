#ifndef TARANTOOL_BOX_TUPLE_H_INCLUDED
#define TARANTOOL_BOX_TUPLE_H_INCLUDED
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
#include "tarantool/util.h"
#include "key_def.h"
#include <pickle.h>
#include "comphint.h"

struct tbuf;

/**
 * @brief In-memory tuple format
 */
struct tuple_format {
	uint16_t id;
	/**
	 * Max field no which participates in any of the space
	 * indexes. Each tuple of this format must have,
	 * therefore, at least max_fieldno fields.
	 *
	 */
	uint32_t max_fieldno;
	/* Length of 'types' and 'offset' arrays. */
	uint32_t field_count;
	/**
	 * Field types of indexed fields. This is an array of size
	 * field_count. If there are gaps, i.e. fields that do not
	 * participate in any index and thus we cannot infer their
	 * type, then respective array members have value UNKNOWN.
	 */
	enum field_type *types;
	/**
	 * Each tuple has an area with field offsets. This area
	 * is located in front of the tuple. It is used to quickly
	 * find field start inside tuple data. This area only
	 * stores offsets of fields preceded with fields of
	 * dynamic length. If preceding fields have a fixed
	 * length, field offset can be calculated once for all
	 * tuples and thus is stored directly in the format object.
	 * The variable below stores the size of field map in the
	 * tuple, *in bytes*.
	 */
	uint32_t field_map_size;
	/**
	 * For each field participating in an index, the format
	 * may either store the fixed offset of the field
	 * (identical in all tuples with this format), or an
	 * offset in the dynamic offset map (field_map), which,
	 * in turn, stores the offset of the field (such offset is
	 * varying between different tuples of the same format).
	 * If an offset is fixed, it's positive, so that
	 * tuple->data[format->offset[fieldno] gives the
	 * start of the field.
	 * If it is varying, it's negative, so that
	 * tuple->data[((uint32_t *) * tuple)[format->offset[fieldno]]]
	 * gives the start of the field.
	 */
	int32_t offset[0];
};

extern struct tuple_format **tuple_formats;
/**
 * Default format for a tuple which does not belong
 * to any space and is stored in memory.
 */
extern struct tuple_format *tuple_format_ber;


static inline uint32_t
tuple_format_id(struct tuple_format *format)
{
	assert(tuple_formats[format->id] == format);
	return format->id;
}

/**
 * @brief Allocate, construct and register a new in-memory tuple
 *	 format.
 * @param space description
 *
 * @return tuple format
 */
struct tuple_format *
tuple_format_new(struct key_def *key_def, uint32_t key_count);

/**
 * An atom of Tarantool/Box storage. Consists of a list of fields.
 * The first field is always the primary key.
 */
struct tuple
{
	/** snapshot generation version */
	uint32_t version;
	/** reference counter */
	uint16_t refs;
	/** format identifier */
	uint16_t format_id;
	/** length of the variable part of the tuple */
	uint32_t bsize;
	/** number of fields in the variable part. */
	uint32_t field_count;
	/**
	 * Fields can have variable length, and thus are packed
	 * into a contiguous byte array. Each field is prefixed
	 * with BER-packed field length.
	 */
	char data[0];
} __attribute__((packed));

/** Allocate a tuple
 *
 * @param size  tuple->bsize
 * @post tuple->refs = 1
 */
struct tuple *
tuple_alloc(struct tuple_format *format, size_t size);

/**
 * Create a new tuple from a sequence of BER-len encoded fields.
 * tuple->refs is 0.
 *
 * @post *data is advanced to the length of tuple data
 *
 * Throws an exception if tuple format is incorrect.
 */
struct tuple *
tuple_new(struct tuple_format *format, uint32_t field_count,
	  const char **data, const char *end);

/**
 * Change tuple reference counter. If it has reached zero, free the tuple.
 *
 * @pre tuple->refs + count >= 0
 */
void
tuple_ref(struct tuple *tuple, int count);

/**
* @brief Return a tuple format instance
* @param tuple tuple
* @return tuple format instance
*/
/*
static inline struct tuple_format *
tuple_format(const struct tuple *tuple)
{
	struct tuple_format *format = tuple_formats[tuple->format_id];
	assert(tuple_format_id(format) == tuple->format_id);
	return format;
}
*/
#define tuple_format(tuple) ((struct tuple_format * const)(tuple_formats[(tuple)->format_id]))

/**
 * Get a field from tuple by index.
 * Returns a pointer to BER-length prefixed field.
 *
 * @pre field < tuple->field_count.
 * @returns field data if field exists or NULL
 */
static inline const char *
tuple_field_old(const struct tuple_format *format,
		const struct tuple *tuple, uint32_t i)
{
	const char *field = tuple->data;

	if (i == 0)
		return field;
	i--;
	if (i < format->max_fieldno) {
		if (format->offset[i] > 0)
			return field + format->offset[i];
		if (format->offset[i] != INT32_MIN) {
			uint32_t *field_map = (uint32_t *) tuple;
			int32_t idx = format->offset[i];
			return field + field_map[idx];
		}
	}
	const char *tuple_end = field + tuple->bsize;

	while (field < tuple_end) {
		uint32_t len = load_varint32(&field);
		field += len;
		if (i == 0)
			return field;
		i--;
	}
	return tuple_end;
}

/**
 * @brief Return field data of the field
 * @param tuple tuple
 * @param field_no field number
 * @param field pointer where the start of field data will be stored,
 *        or NULL if field is out of range
 * @param len pointer where the len of the field will be stored
 */
static inline const char *
tuple_field(const struct tuple *tuple, uint32_t i, uint32_t *len)
{
	const char *field = tuple_field_old(tuple_format(tuple), tuple, i);
	if (field < tuple->data + tuple->bsize) {
		*len = load_varint32(&field);
		return field;
	}
	return NULL;
}

/**
 * @brief Tuple Interator
 */
struct tuple_iterator {
	/** @cond false **/
	/* State */
	const struct tuple *tuple;
	/** Always points to the beginning of the next field. */
	const char *pos;
	/** @endcond **/
};

/**
 * @brief Initialize an iterator over tuple fields
 *
 * A workflow example:
 * @code
 * struct tuple_iterator it;
 * tuple_rewind(&it, tuple);
 * const char *field;
 * uint32_t len;
 * while ((field = tuple_next(&it, &len)))
 *	lua_pushlstring(L, field, len);
 *
 * @endcode
 *
 * @param[out] it tuple iterator
 * @param[in]  tuple tuple
 */
static inline void
tuple_rewind(struct tuple_iterator *it, const struct tuple *tuple)
{
	it->tuple = tuple;
	it->pos = tuple->data;
}

/**
 * @brief Position the iterator at a given field no.
 *
 * @retval field  if the iterator has the requested field
 * @retval NULL   otherwise (iteration is out of range)
 */
const char *
tuple_seek(struct tuple_iterator *it, uint32_t field_no, uint32_t *len);

/**
 * @brief Iterate to the next field
 * @param it tuple iterator
 * @return next field or NULL if the iteration is out of range
 */
const char *
tuple_next(struct tuple_iterator *it, uint32_t *len);

/**
 * @brief Print a tuple in yaml-compatible mode to tbuf:
 * key: { value, value, value }
 *
 * @param buf tbuf
 * @param tuple tuple
 */
void
tuple_print(struct tbuf *buf, const struct tuple *tuple);

struct tuple *
tuple_update(struct tuple_format *new_format,
	     void *(*region_alloc)(void *, size_t), void *alloc_ctx,
	     const struct tuple *old_tuple,
	     const char *expr, const char *expr_end);

/** Tuple length when adding to iov. */
static inline size_t tuple_len(struct tuple *tuple)
{
	return tuple->bsize + sizeof(tuple->bsize) +
		sizeof(tuple->field_count);
}

static inline size_t
tuple_range_size(const char **begin, const char *end, uint32_t count)
{
	const char *start = *begin;
	while (*begin < end && count-- > 0) {
		size_t len = load_varint32(begin);
		*begin += len;
	}
	return *begin - start;
}

void tuple_free(struct tuple *tuple);

/**
 * @brief Compare two tuples using field by field using key definition
 * @param tuple_a tuple
 * @param tuple_b tuple
 * @param key_def key definition
 * @retval 0  if key_fields(tuple_a) == key_fields(tuple_b)
 * @retval <0 if key_fields(tuple_a) < key_fields(tuple_b)
 * @retval >0 if key_fields(tuple_a) > key_fields(tuple_b)
 */
int
tuple_compare(const struct tuple *tuple_a, const struct tuple *tuple_b,
	      const struct key_def *key_def);

/**
 * @brief Compare two tuples using field by field using key definition
 * @param tuple_a tuple
 * @param tuple_b tuple
 * @param key_def key definition
 * @param hint compare hint
 * @retval 0  if key_fields(tuple_a) == key_fields(tuple_b)
 * @retval <0 if key_fields(tuple_a) < key_fields(tuple_b)
 * @retval >0 if key_fields(tuple_a) > key_fields(tuple_b)
 */
int
tuple_compare_hint(const struct tuple *tuple_a, const struct tuple *tuple_b,
		   const struct key_def *key_def, compare_hint *hint);

int
tuple_compare_hint_dbg(const struct tuple *tuple_a, const struct tuple *tuple_b,
		   const struct key_def *key_def, compare_hint *hint);

/**
 * @brief Compare two tuples field by field for duplicate using key definition
 * @param tuple_a tuple
 * @param tuple_b tuple
 * @param key_def key definition
 * @retval 0  if key_fields(tuple_a) == key_fields(tuple_b) and
 * tuple_a == tuple_b - tuple_a is the same object as tuple_b
 * @retval <0 if key_fields(tuple_a) <= key_fields(tuple_b)
 * @retval >0 if key_fields(tuple_a > key_fields(tuple_b)
 */
int
tuple_compare_dup(const struct tuple *tuple_a, const struct tuple *tuple_b,
		  const struct key_def *key_def);

/**
 * @brief Compare two tuples field by field for duplicate using key definition
 * @param tuple_a tuple
 * @param tuple_b tuple
 * @param key_def key definition
 * @param hint compare hint
 * @retval 0  if key_fields(tuple_a) == key_fields(tuple_b) and
 * tuple_a == tuple_b - tuple_a is the same object as tuple_b
 * @retval <0 if key_fields(tuple_a) <= key_fields(tuple_b)
 * @retval >0 if key_fields(tuple_a > key_fields(tuple_b)
 */
int
tuple_compare_dup_hint(const struct tuple *tuple_a, const struct tuple *tuple_b,
		       const struct key_def *key_def, compare_hint *hint);
int
tuple_compare_dup_hint_dbg(const struct tuple *tuple_a, const struct tuple *tuple_b,
		       const struct key_def *key_def, compare_hint *hint);

/**
 * @brief Compare a tuple with a key field by field using key definition
 * @param tuple_a tuple
 * @param key BER-encoded key
 * @param part_count number of parts in \a key
 * @param key_def key definition
 * @retval 0  if key_fields(tuple_a) == parts(key)
 * @retval <0 if key_fields(tuple_a) < parts(key)
 * @retval >0 if key_fields(tuple_a) > parts(key)
 */
int
tuple_compare_with_key(const struct tuple *tuple_a, const char *key,
		       uint32_t part_count, const struct key_def *key_def);

/**
 * @brief Compare a tuple with a key field by field using key definition
 * @param tuple_a tuple
 * @param key BER-encoded key
 * @param part_count number of parts in \a key
 * @param key_def key definition
 * @param hint compare hint
 * @retval 0  if key_fields(tuple_a) == parts(key)
 * @retval <0 if key_fields(tuple_a) < parts(key)
 * @retval >0 if key_fields(tuple_a) > parts(key)
 */
int
tuple_compare_with_key_hint(const struct tuple *tuple_a, const char *key,
			    uint32_t part_count,
			    const struct key_def *key_def, compare_hint *hint);
int
tuple_compare_with_key_hint_dbg(const struct tuple *tuple_a, const char *key,
			    uint32_t part_count,
			    const struct key_def *key_def, compare_hint *hint);

int
mymemcmp(const void * ptr1, const void * ptr2, size_t count);

template<enum field_type type>
static inline int
tuple_compare_field_hint_tpl(const char *field_a, const char *field_b,
		    uint32_t *hint)
{
	if (type == NUM) {
		assert(field_a[0] == field_b[0]);
		uint32_t a = *(uint32_t *) (field_a + 1);
		uint32_t b = *(uint32_t *) (field_b + 1);
		/*
		 * Little-endian unsigned int is memcmp
		 * compatible.
		 */
		return a < b ? -1 : a > b;
	}
	if (type == NUM64) {
		assert(field_a[0] == field_b[0]);
		uint64_t a = *(uint64_t *) (field_a + 1);
		uint64_t b = *(uint64_t *) (field_b + 1);
		return a < b ? -1 : a > b;
	}
	if (type != NUM && type != NUM64) {
		uint32_t size_a = load_varint32(&field_a);
		uint32_t size_b = load_varint32(&field_b);
		uint32_t size_min = MIN(size_a, size_b);
		int r = mymemcmp(field_a + *hint, field_b + *hint,
			(size_t)(size_min - *hint));
		if (r == 0) {
			r = size_a < size_b ? -1 : size_a > size_b;
			*hint = size_min;
		} else {
			*hint += (r > 0 ? r : -r) - 1;
		}
		return r;
	}
}

template<enum field_type type>
struct enum_field_type_to_index;
template<>
struct enum_field_type_to_index<NUM> {
	enum tag_index { index = 0 };
};
template<>
struct enum_field_type_to_index<NUM64> {
	enum tag_index { index = 1 };
};
template<>
struct enum_field_type_to_index<STRING> {
	enum tag_index { index = 2 };
};

template<int index>
struct index_to_enum_field_type;
template<>
struct index_to_enum_field_type<0> {
	enum tag_type { type = NUM };
};
template<>
struct index_to_enum_field_type<1> {
	enum tag_type { type = NUM64 };
};
template<>
struct index_to_enum_field_type<2> {
	enum tag_type { type = STRING };
};

const int field_type_count = 3;

template<int field_count, int skip_fields, int types_info>
struct TupleComparator;

template<int skip_fields, int types_info>
struct TupleComparator<skip_fields, skip_fields, types_info> {
	static int
	Compare(const struct tuple *, const struct tuple *,
		const struct tuple_format *,
		const struct tuple_format *,
		struct key_part **part, compare_hint *) {
		*part += skip_fields;
		return 0;
	}
};

template<int field_count, int skip_fields, int types_info>
struct TupleComparator {
	static int
	Compare(const struct tuple *tuple_a, const struct tuple *tuple_b,
		const struct tuple_format *format_a,
		const struct tuple_format *format_b,
		struct key_part **part, compare_hint *hint)
	{
		typedef TupleComparator<field_count - 1, skip_fields, types_info / field_type_count> P;
		int r = P::Compare(tuple_a, tuple_b, format_a, format_b, part, hint);
		if (r) {
			return r;
		} else if (field_count > 1) {
			hint->fields_hit = field_count - 1;
			hint->current_field_hit = 0;
		}
		const char *field_a = tuple_field_old(format_a, tuple_a, (*part)->fieldno);
		const char *field_b = tuple_field_old(format_b, tuple_b, (*part)->fieldno);
		(*part)++;
		const int cur_ti = types_info % field_type_count;
		const enum field_type cur_ft = (enum field_type)index_to_enum_field_type<cur_ti>::type;
		r = tuple_compare_field_hint_tpl<cur_ft>(field_a, field_b, &hint->current_field_hit);
		if (r == 0) {
			hint->fields_hit++;
			hint->current_field_hit = 0;
		}
		return r;
	}
};

typedef int
(*Comparer)(const struct tuple *tuple_a, const struct tuple *tuple_b,
	const struct tuple_format *format_a,
	const struct tuple_format *format_b,
	struct key_part **part, compare_hint *hint);


template<int field_count, int skip_fields, int types_info>
struct SkipInstaniator;

template<int field_count, int types_info>
struct SkipInstaniator<field_count, 0, types_info> {
	SkipInstaniator<field_count, 0, types_info>(Comparer* arr) {
		arr[0] = TupleComparator<field_count, 0, types_info>::Compare;
	}
};

template<int field_count, int skip_fields, int types_info>
struct SkipInstaniator
	: SkipInstaniator<field_count, skip_fields - 1, types_info> {
	SkipInstaniator<field_count, skip_fields, types_info> (Comparer* arr)
		: SkipInstaniator<field_count, skip_fields - 1, types_info>(arr) {
		arr[skip_fields] = TupleComparator<field_count, skip_fields, types_info>::Compare;
	}
};

#include <signal.h>

template<int field_count, int types_info>
inline int
tuple_compare_hint_tpl(const struct tuple *tuple_a, const struct tuple *tuple_b,
		       const struct key_def *key_def, compare_hint *hint)
{
	static Comparer skip_func_arr[field_count + 1];
	static const SkipInstaniator<field_count, field_count, types_info> inst(skip_func_arr);

	struct key_part *part = key_def->parts;
	struct tuple_format *format_a = tuple_format(tuple_a);
	struct tuple_format *format_b = tuple_format(tuple_b);

	int r = skip_func_arr[hint->fields_hit](tuple_a, tuple_b, format_a, format_b, &part, hint);

	return r;
}

template<int field_count, int types_info>
inline int
tuple_compare_dup_hint_tpl(const struct tuple *tuple_a, const struct tuple *tuple_b,
		       const struct key_def *key_def, compare_hint *hint)
{
	int r = tuple_compare_hint_tpl<field_count, types_info>(tuple_a, tuple_b, key_def, hint);
	if (r == 0)
		r = tuple_a < tuple_b ? -1 : tuple_a > tuple_b;

	return r;
}

/** These functions are implemented in tuple_convert.cc. */

/* Store tuple in the output buffer in iproto format. */
void
tuple_to_obuf(struct tuple *tuple, struct obuf *buf);

/* Store tuple fields in the Lua buffer, BER-length-encoded. */
void
tuple_to_luabuf(struct tuple *tuple, struct luaL_Buffer *b);

/** Initialize tuple library */
void
tuple_format_init();

/** Cleanup tuple library */
void
tuple_format_free();
#endif /* TARANTOOL_BOX_TUPLE_H_INCLUDED */

