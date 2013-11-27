#ifdef ENABLE_DTRACE
#include "cjson_dtrace.h"
#ifdef ENABLE_DTRACE_BUILTIN
#define	LUA_CJSON_START() TARANTOOL_START()
#define	LUA_CJSON_START_ENABLED() TARANTOOL_START_ENABLED()
#define	LUA_CJSON_END(arg0, arg1) TARANTOOL_END(arg0, arg1)
#define	LUA_CJSON_END_ENABLED() TARANTOOL_END_ENABLED()
#endif
#else
#define	LUA_CJSON_START_ENABLED() (0)
#define LUA_CJSON_START()
#define	LUA_CJSON_END_ENABLED() (0)
#define LUA_CJSON_END(arg0, arg1)
#endif