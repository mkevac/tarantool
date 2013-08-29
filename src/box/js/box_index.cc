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

#include "box.h"
#include "box_index.h"
#include "box_space.h"
#include "box_tuple.h"

#include <palloc.h>
#include <fiber.h>

#include "../space.h"
#include "../index.h"

namespace js {
namespace box {
namespace index {

namespace { /* (anonymous) */

enum {
	INTERNAL_SPACE_ID = 0,
	INTERNAL_INDEX_ID = 1,
	INTERNAL_TUPLE_FUN = 2,
	INTERNAL_MAX = 3
};

inline class Index *
Unwrap(v8::Local<v8::Object> thiz)
{
	assert(thiz->InternalFieldCount() > INTERNAL_INDEX_ID);
	uint32_t space_id = thiz->GetInternalField(INTERNAL_SPACE_ID)->
			    Uint32Value();
	uint32_t index_id = thiz->GetInternalField(INTERNAL_SPACE_ID)->
			    Uint32Value();

	struct space *space = space_find(space_id);
	class Index *index = index_find(space, index_id);
	return index;
}

inline void
ReturnTuple(const v8::FunctionCallbackInfo<v8::Value>& args, struct tuple *tuple)
{
	v8::HandleScope handle_scope;

	if (tuple == NULL)
		return;

	v8::Local<v8::Object> thiz = args.Holder();
	assert(thiz->InternalFieldCount() > INTERNAL_TUPLE_FUN);
	v8::Local<v8::Function> tuple_fun =
			thiz->GetInternalField(INTERNAL_TUPLE_FUN).
			As<v8::Function>();

	args.GetReturnValue().Set(tuple::NewInstance(tuple_fun, tuple));
}

void
MinMethod(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	JS_BEGIN();

	v8::HandleScope handle_scope;

	class Index *index = Unwrap(args.Holder());
	ReturnTuple(args, index->min());

	JS_END();
}

void
MaxMethod(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	JS_BEGIN();

	v8::HandleScope handle_scope;

	class Index *index = Unwrap(args.Holder());
	ReturnTuple(args, index->max());

	JS_END();
}

void
RandomMethod(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	JS_BEGIN();

	v8::HandleScope handle_scope;

	if (args.Length() < 1 || !args[0]->IsUint32()) {
		v8::ThrowException(v8::Exception::Error(
			v8::String::New("Usage: random(rnd: Uint32)")));
		return;
	}

	uint32_t rnd = args[0]->Uint32Value();
	class Index *index = Unwrap(args.Holder());
	ReturnTuple(args, index->random(rnd));

	JS_END();
}

void
LengthPropertyGet(v8::Local<v8::String> property,
		 const v8::PropertyCallbackInfo<v8::Value>& args)
{
	(void) property;

	class Index *index = Unwrap(args.Holder());
	args.GetReturnValue().Set(v8::Uint32::New(index->size()));
}


void
CountMethod(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	size_t allocated_size = palloc_allocated(fiber->gc_pool);

	JS_BEGIN();

	if (args.Length() < 1 || !args[0]->IsArray()) {
		v8::ThrowException(v8::Exception::Error(
			v8::String::New("Usage: count(key)")));
		return;
	}

	class Index *index = Unwrap(args.Holder());

	v8::Local<v8::Array> key_arg = args[0].As<v8::Array>();
	assert(!key_arg.IsEmpty());
	uint32_t key_len = BerSizeOf(key_arg);
	if (key_len == UINT32_MAX)
		return;

	char *key = (char *) palloc(fiber->gc_pool, key_len);
	char *key_end = key + key_len;
	char *b = key;

	b = BerPack(b, key_arg);  /* Key */
	assert(b == key_end);
	uint32_t key_part_count = key_arg->Length();

	uint32_t count = 0;

	key_validate(&index->key_def, ITER_EQ, key, key_part_count);
	/* Prepare index iterator */
	struct iterator *it = index->position();
	index->initIterator(it, ITER_EQ, key, key_part_count);
	/* Iterate over the index and count tuples. */
	struct tuple *tuple;
	while ((tuple = it->next(it)) != NULL)
		count++;

	args.GetReturnValue().Set(count);

	JS_END();

	ptruncate(fiber->gc_pool, allocated_size);
}

void
Call(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::HandleScope handle_scope;

	if (!args.IsConstructCall()) {
		v8::ThrowException(v8::Exception::Error(
			v8::String::New("Please use 'new'!")));
		return;
	}

	if (args.Length() < 1 || !args[0]->IsExternal() ||
	    !args[1]->IsExternal()) {
		v8::ThrowException(v8::Exception::Error(
			v8::String::New("Usage: new Index(space: Space, index: Index)")));
		return;
	}

	v8::Local<v8::Object> thiz = args.Holder();

	struct space *space = (struct space *)
		args[0].As<v8::External>()->Value();
	class Index *index = (class Index *)
		args[1].As<v8::External>()->Value();

	struct key_def *key_def = &index->key_def;

	uint32_t space_id = space->def.id;
	uint32_t index_id = ::index_id(index);

	thiz->SetInternalField(INTERNAL_SPACE_ID, v8::Uint32::New(space_id));
	thiz->SetInternalField(INTERNAL_INDEX_ID, v8::Uint32::New(index_id));
	thiz->SetInternalField(INTERNAL_TUPLE_FUN, tuple::Exports());

	thiz->Set(v8::String::NewSymbol("space_id"), v8::Uint32::New(space_id),
		  (v8::PropertyAttribute) (v8::ReadOnly | v8::DontEnum));

	thiz->Set(v8::String::NewSymbol("id"),
		  v8::Uint32::New(index_id), v8::ReadOnly);

	thiz->Set(v8::String::NewSymbol("unique"),
		  v8::Boolean::New(key_def->is_unique), v8::ReadOnly);

	thiz->Set(v8::String::NewSymbol("type"),
		  v8::String::NewSymbol(index_type_strs[key_def->type]),
		  v8::ReadOnly);

	v8::Local<v8::Array> key_fields = v8::Array::New(key_def->part_count);

	thiz->Set(v8::String::NewSymbol("key_field"), key_fields, v8::ReadOnly);
	for (uint32_t j = 0; j < key_def->part_count; j++) {
		v8::Local<v8::Object> key_field = v8::Object::New();
		auto type = field_type_strs[key_def->parts[j].type];
		key_field->Set(v8::String::NewSymbol("type"),
			v8::String::NewSymbol(type), v8::ReadOnly);

		key_field->Set(v8::String::NewSymbol("field_id"),
			v8::Uint32::New(key_def->parts[j].fieldno),
			v8::ReadOnly);

		key_fields->Set(v8::Uint32::New(j), key_field, v8::ReadOnly);
	}
}

v8::Local<v8::FunctionTemplate>
GetTemplate()
{
	v8::HandleScope handle_scope;

	intptr_t tmpl_key = (intptr_t) &GetTemplate;
	JS *js = JS::GetCurrent();
	v8::Local<v8::FunctionTemplate> tmpl = js->TemplateCacheGet(tmpl_key);
	if (!tmpl.IsEmpty())
		return handle_scope.Close(tmpl);

	tmpl = v8::FunctionTemplate::New(Call);

	tmpl->SetClassName(v8::String::NewSymbol("Box.Index"));
	tmpl->InstanceTemplate()->SetInternalFieldCount(INTERNAL_MAX);

	tmpl->InstanceTemplate()->SetAccessor(v8::String::NewSymbol("length"),
		LengthPropertyGet, 0, v8::Local<v8::Value>(),
		v8::DEFAULT, v8::DontEnum);

	tmpl->PrototypeTemplate()->Set(
		v8::String::NewSymbol("min"),
		v8::FunctionTemplate::New(MinMethod));

	tmpl->PrototypeTemplate()->Set(
		v8::String::NewSymbol("max"),
		v8::FunctionTemplate::New(MaxMethod));

	tmpl->PrototypeTemplate()->Set(
		v8::String::NewSymbol("random"),
		v8::FunctionTemplate::New(RandomMethod));

	tmpl->PrototypeTemplate()->Set(
		v8::String::NewSymbol("count"),
		v8::FunctionTemplate::New(CountMethod));

	js->TemplateCacheSet(tmpl_key, tmpl);
	return handle_scope.Close(tmpl);
}

} /* namespace (anonymous) */

v8::Local<v8::Object>
Exports()
{
	return GetTemplate()->GetFunction();
}

v8::Local<v8::Object>
NewInstance(v8::Local<v8::Function> index_fun, struct space *space,
            class Index *index)
{
	v8::HandleScope handle_scope;

	v8::Local<v8::Value> argv[] = {
		v8::External::New(space),
		v8::External::New(index)
	};

	return handle_scope.Close(index_fun->NewInstance(2, argv));
}

} /* namespace index */
} /* namespace box */
} /* namespace js */
