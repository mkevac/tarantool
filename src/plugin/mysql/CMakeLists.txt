find_optional_package(MySQL)

if (MYSQL_FOUND)
	message(STATUS "box.net.sql(mysql) INC=${MYSQL_INCLUDE_DIRS}")
	message(STATUS "box.net.sql(mysql) LIBS=${MYSQL_LIBRARIES}")
	
	include_directories(${MYSQL_INCLUDE_DIRS})
	add_library(mysql SHARED mysql.cc)
	target_link_libraries(mysql ${MYSQL_LIBRARIES} -rdynamic)
	install(TARGETS mysql LIBRARY DESTINATION ${PLUGIN_DIR})
else()
	message(STATUS "MySQL client not found, box.net.sql(mysql) disabled")
endif()
