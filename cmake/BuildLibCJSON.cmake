#
# A macro to build the bundled liblua-cjson
macro(libcjson_build)
    set(cjson_src ${PROJECT_SOURCE_DIR}/third_party/lua-cjson/lua_cjson.c 
                  ${PROJECT_SOURCE_DIR}/third_party/lua-cjson/strbuf.c 
                  ${PROJECT_SOURCE_DIR}/third_party/lua-cjson/fpconv.c)

    add_library(cjson STATIC ${cjson_src})

    if (ENABLE_DTRACE)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                  ${PROJECT_SOURCE_DIR}/extra/dtrace/cjson_external.h
                  ${PROJECT_SOURCE_DIR}/third_party/lua-cjson/external.h
        )
    endif()

    if (ENABLE_DTRACE AND NOT TARGET_OS_DARWIN)
        set(cjson_obj_dir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/cjson.dir/third_party/lua-cjson)
        set(cjson_objs
            ${cjson_obj_dir}/fpconv.c.o
            ${cjson_obj_dir}/lua_cjson.c.o
            ${cjson_obj_dir}/strbuf.c.o
        )
	set(LIBCJSON_LIBRARIES ${cjson_objs})

        unset(cjson_obj_dir)
        unset(cjson_objs)
    else()
        set(LIBCJSON_LIBRARIES cjson)
    endif()

    set(LIBCJSON_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party/lua-cjson)

    message(STATUS "Use bundled Lua-CJSON library: ${LIBCJSON_LIBRARIES}")

    unset(lua_cjson_src)
endmacro(libcjson_build)

