find_program(DTRACE dtrace)

macro(dtrace_gen_h provider header)
    message(STATUS "DTrace generate ${header}")
    execute_process(
        COMMAND ${DTRACE} -h -s ${provider} -o ${header}
    )
endmacro(dtrace_gen_h)

if(DTRACE)
    set(DTRACE_FOUND ON)
endif(DTRACE)

if(DTRACE_FOUND AND ENABLE_DTRACE)
    add_definitions(-DENABLE_DTRACE)
    message(STATUS "DTrace found")
    set(DTRACE_O_DIR ${CMAKE_CURRENT_BINARY_DIR}/dtrace)
    if(ENABLE_DTRACE_BUILTIN)
        add_definitions(-DENABLE_DTRACE_BUILTIN)
        set(DTRACE_D_FILE ${PROJECT_SOURCE_DIR}/include/builtin_provider.d)
    else()
        set(DTRACE_D_FILE ${PROJECT_SOURCE_DIR}/include/tarantool_provider.d)
    endif(ENABLE_DTRACE_BUILTIN)
    execute_process(COMMAND mkdir ${DTRACE_O_DIR})
    message(STATUS "DTrace obj dir ${DTRACE_O_DIR}")
    dtrace_gen_h(${DTRACE_D_FILE} ${PROJECT_SOURCE_DIR}/include/tarantool_provider.h)
    set (dtrace_headers
        lua-cjson/cjson_dtrace.h
        coro/coro_dtrace.h
        libev/ev_dtrace.h
    )
    foreach(dtrace_header ${dtrace_headers})
       dtrace_gen_h(${DTRACE_D_FILE} ${PROJECT_SOURCE_DIR}/third_party/${dtrace_header})
    endforeach(dtrace_header)

    unset(dtrace_headers)
else(DTRACE_FOUND AND ENABLE_DTRACE)
    message(FATAL_ERROR "Could not find DTrace")
endif (DTRACE_FOUND AND ENABLE_DTRACE)

