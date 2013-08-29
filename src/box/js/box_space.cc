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
#include "box_space.h"
#include "box_index.h"

#include "../space.h"
#include "../index.h"

namespace js {
namespace box {
namespace space {

namespace { /* (anonymous) */

void
Call(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::HandleScope handle_scope;

	if (!args.IsConstructCall()) {
		v8::ThrowException(v8::Exception::Error(
			v8::String::New("Please use 'new'!")));

		return;
	}

	if (args.Length() < 0 || !args[0]->IsExternal()) {
		v8::ThrowException(v8::Exception::Error(
			v8::String::New("Usage: new Space(space: Space)")));
		return;
	}

	struct space *space = (struct space *)
		args[0].As<v8::External>()->Value();

	v8::Local<v8::Object> thiz = args.This();

	thiz->Set(v8::String::NewSymbol("id"),
		  v8::Uint32::New(space->def.id), v8::ReadOnly);
	thiz->Set(v8::String::NewSymbol("arity"),
		  v8::Uint32::New(space->def.arity), v8::ReadOnly);
	thiz->Set(v8::String::NewSymbol("enabled"),
		  v8::Boolean::New(true), v8::ReadOnly);

	v8::Local<v8::Array> indexes = v8::Array::New(space->index_id_max);
	thiz->Set(v8::String::NewSymbol("index"), indexes, v8::ReadOnly);

	v8::Local<v8::Function> index_fun = index::Exports().As<v8::Function>();
	assert(!index_fun.IsEmpty());
	for (int i = 0; i <= space->index_id_max; i++) {
		class Index *index = space_index(space, i);
		if (index == NULL)
			continue;

		indexes->Set(v8::Uint32::New(i),
			     index::NewInstance(index_fun, space, index),
			     v8::ReadOnly);
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

	tmpl->SetClassName(v8::String::NewSymbol("Box.Space"));

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
NewInstance(v8::Local<v8::Function> space_fun, struct space *space)
{
	v8::HandleScope handle_scope;

	v8::Local<v8::Value> argv[] = {
		v8::External::New(space)
	};

	return handle_scope.Close(space_fun->NewInstance(1, argv));
}

} /* namespace space */
} /* namespace box */
} /* namespace js */
