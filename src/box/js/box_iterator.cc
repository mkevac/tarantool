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
#include "box_tuple.h"

#include <salloc.h>
#include <scoped_guard.h>

#include "../space.h"
#include "../index.h"

namespace js {
namespace box {
namespace iterator {

namespace { /* (anonymous) */

enum {
	INTERNAL_ITERATOR = 0,
	INTERNAL_TUPLE_FUN = 1,
	INTERNAL_MAX = 2
};

inline struct iterator *
Unwrap(v8::Local<v8::Object> thiz)
{
	assert(thiz->InternalFieldCount() > INTERNAL_ITERATOR);
	struct iterator *iterator = (struct iterator *)
		thiz-> GetAlignedPointerFromInternalField(INTERNAL_ITERATOR);
	assert(iterator != NULL);
	return iterator;
}


void
GC(v8::Isolate* isolate, v8::Persistent<v8::Object>* value, char *key)
{
	(void) isolate;

	v8::HandleScope handle_scope;

	v8::Local<v8::Object> thiz = v8::Local<v8::Object>::New(isolate,*value);
	struct iterator *it = Unwrap(thiz);
	it->free(it);

	/* TODO: fix calculation here */
	v8::V8::AdjustAmountOfExternalAllocatedMemory(sizeof(struct iterator));

	if (key != NULL)
		sfree(key);

	assert(value->IsNearDeath());
	value->Dispose();
	value->Clear();
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

	if (args.Length() < 3 || !args[0]->IsUint32() || !args[1]->IsUint32() ||
	    !args[2]->IsUint32()) {
		v8::ThrowException(v8::Exception::Error(
			v8::String::New("Usage new Iterator(space, index, "
					" type, [key])")));
		return;
	}

	v8::Local<v8::Object> thiz = args.This();

	JS_BEGIN();

	struct space *space = space_find(args[0]->Uint32Value());
	class Index *index = index_find(space, args[1]->Uint32Value());
	enum iterator_type type = (enum iterator_type) args[2]->Uint32Value();

	char *key = NULL;
	uint32_t key_part_count = 0;
	if (args[3]->IsArray()) {
		v8::Local<v8::Array> key_arg = args[3].As<v8::Array>();
		assert(!key_arg.IsEmpty());
		uint32_t key_len = BerSizeOf(key_arg);
		if (key_len == UINT32_MAX)
			return;

		key = (char *) salloc(key_len, "iterator key");
		char *key_end = key + key_len;
		char *b = key;

		b = BerPack(b, key_arg);
		assert(b == key_end);
		key_part_count = key_arg->Length();
	}

	auto key_guard = make_scoped_guard([=] {
		if (key != NULL)
			sfree(key);
	});

	/* Validate the key */
	key_validate(&index->key_def, type, key, key_part_count);

	/* Prepare index iterator */
	struct iterator *it = index->allocIterator();
	auto it_guard = make_scoped_guard([=] {
		it->free(it);
	});

	index->initIterator(it, type, key, key_part_count);

	thiz->SetAlignedPointerInInternalField(INTERNAL_ITERATOR, it);
	thiz->SetInternalField(INTERNAL_TUPLE_FUN, tuple::GetConstructor());

	/* TODO: fix calculation here */
	v8::V8::AdjustAmountOfExternalAllocatedMemory(sizeof(struct iterator));

	v8::Persistent<v8::Object> handle(v8::Isolate::GetCurrent(), thiz);

	/* Set v8 GC callback */
	handle.MakeWeak(key, GC);
	handle.MarkIndependent();

	key_guard.reset();
	it_guard.reset();

	JS_END();
}

void
Invoke(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::HandleScope handle_scope;

	struct iterator *it = Unwrap(args.Holder());

	struct tuple *tuple = it->next(it);
	if (tuple == NULL)
		return;

	v8::Local<v8::Object> thiz = args.Holder();
	assert(thiz->InternalFieldCount() > INTERNAL_TUPLE_FUN);
	v8::Local<v8::Function> tuple_fun =
			thiz->GetInternalField(INTERNAL_TUPLE_FUN).
			As<v8::Function>();

	args.GetReturnValue().Set(tuple::NewInstance(tuple_fun, tuple));
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

	tmpl->SetClassName(v8::String::NewSymbol("Box.Iterator"));
	tmpl->InstanceTemplate()->SetInternalFieldCount(INTERNAL_MAX);
	tmpl->InstanceTemplate()->SetCallAsFunctionHandler(Invoke);

	for (int i = 0; i < iterator_type_MAX; i++) {
		assert(strncmp(iterator_type_strs[i], "ITER_", 5) == 0);
		tmpl->Set(v8::String::NewSymbol(iterator_type_strs[i] + 5),
			  v8::Uint32::New(i), v8::ReadOnly);
	}

	js->TemplateCacheSet(tmpl_key, tmpl);
	return handle_scope.Close(tmpl);
}

} /* namespace (anonymous) */

v8::Local<v8::Object>
Exports()
{
	return GetTemplate()->GetFunction();
}

} /* namespace iterator */
} /* namespace box */
} /* namespace js */
