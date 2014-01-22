
find_path(SOPHIA_INCLUDE_DIR NAMES sophia.h
          PATH_SUFFIXES db
		  PATHS ${SOPHIA_ROOT})

find_library(SOPHIA_LIBRARIES NAMES sophia
             PATH_SUFFIXES db
			 PATHS ${SOPHIA_ROOT})

if (SOPHIA_INCLUDE_DIR AND SOPHIA_LIBRARIES)
    set(SOPHIA_FOUND ON)
endif()

if (SOPHIA_FOUND)
    if (NOT SOPHIA_FIND_QUIETLY)
        message(STATUS "Found Sophia includes: ${SOPHIA_INCLUDE_DIR}/sophia.h")
        message(STATUS "Found Sophia library: ${SOPHIA_LIBRARIES}")
    endif ()
    set(SOPHIA_INCLUDE_DIRS ${SOPHIA_INCLUDE_DIR})
else()
    if (SOPHIA_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find Sophia development files")
    endif ()
endif ()
