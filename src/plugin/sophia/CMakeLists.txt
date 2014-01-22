
find_optional_package(Sophia)

if (SOPHIA_FOUND)
	include_directories(${SOPHIA_INCLUDE_DIRS})
	add_library(sophia SHARED sophia.cc)
	target_link_libraries(sophia ${SOPHIA_LIBRARIES} -rdynamic)
	install(TARGETS sophia LIBRARY DESTINATION ${PLUGIN_DIR})
else()
	message(STATUS "Sophia library not found")
endif()
