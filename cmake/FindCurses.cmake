# find Curses includes and library
#
# CURSES_FOUND
# CURSES_LIBRARY
# CURSES_INCLUDE_DIR

if (CMAKE_STATICBUILD)
  set(TMP ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()
if (DEFINED CURSES_ROOT)
  set(_FIND_OPTS NO_CMAKE NO_CMAKE_SYSTEM_PATH)
  find_library(CURSES_LIBRARY
    NAMES curses
    HINTS ${CURSES_ROOT}/lib
    ${_FIND_OPTS})
  find_path(CURSES_INCLUDE_DIR
    NAMES curses.h
    HINTS ${CURSES_ROOT}/include ${_FIND_OPTS})
else()
  find_library(CURSES_LIBRARY NAMES curses)
  find_path(CURSES_INCLUDE_DIR curses.h)
endif()
if (CMAKE_STATICBUILD)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${TMP})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Curses
    REQUIRED_VARS CURSES_INCLUDE_DIR CURSES_LIBRARY)
set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIR})
set(CURSES_LIBRARIES ${CURSES_LIBRARY})

mark_as_advanced(CURSES_LIBRARIES CURSES_INCLUDE_DIRS)
