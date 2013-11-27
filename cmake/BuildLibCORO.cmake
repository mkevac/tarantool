#
# A macro to build the bundled libcoro
macro(libcoro_build)
    set(coro_src
        ${PROJECT_SOURCE_DIR}/third_party/coro/coro.c
    )

    add_library(coro STATIC ${coro_src})

    if (ENABLE_DTRACE AND NOT TARGET_OS_DARWIN)
        set(LIBCORO_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/coro.dir/third_party/coro/coro.c.o)
    else()
        set(LIBCORO_LIBRARIES coro)
    endif()

    set(LIBCORO_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party/coro)

    message(STATUS "Use bundled libcoro includes: ${LIBCORO_INCLUDE_DIR}/coro.h")
    message(STATUS "Use bundled libcoro library: ${LIBCORO_LIBRARIES}")

    unset(coro_src)
endmacro(libcoro_build)

