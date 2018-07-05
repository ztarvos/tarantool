# - Find the readline include files and libraries
# - Include finding of termcap or curses
#
# READLINE_FOUND
# READLINE_INCLUDE_DIR
# READLINE_LIBRARIES
#

find_package(Curses)
if(NOT CURSES_FOUND)
    find_package(Termcap)
endif()

if (CMAKE_STATICBUILD)
  set(TMP ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    find_library(TINFO_LIBRARY NAMES tinfo)
  endif()
endif()
if (DEFINED READLINE_ROOT)
  set(_FIND_OPTS NO_CMAKE NO_CMAKE_SYSTEM_PATH)
  find_library(READLINE_LIBRARY
    NAMES readline
    HINTS ${READLINE_ROOT}/lib
    ${_FIND_OPTS})
  find_path(READLINE_INCLUDE_DIR
    NAMES readline/readline.h
    HINTS ${READLINE_ROOT}/include ${_FIND_OPTS})
else()
  find_library(READLINE_LIBRARY NAMES readline)
  find_path(READLINE_INCLUDE_DIR readline/readline.h)
endif()
if (CMAKE_STATICBUILD)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${TMP})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Readline
    REQUIRED_VARS READLINE_INCLUDE_DIR READLINE_LIBRARY)
set(READLINE_INCLUDE_DIRS ${READLINE_INCLUDE_DIR})
set(READLINE_LIBRARIES ${READLINE_LIBRARY})

if(READLINE_FOUND)
  if(EXISTS ${READLINE_INCLUDE_DIR}/readline/rlconf.h)
      if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        if (TINFO_LIBRARY)
          set(CURSES_LIBRARIES ${CURSES_LIBRARIES} ${TINFO_LIBRARY})
        endif()
      endif()
      set(CMAKE_REQUIRED_LIBRARIES ${CURSES_LIBRARIES})
      check_library_exists(${READLINE_LIBRARY} rl_catch_sigwinch ""
          HAVE_GNU_READLINE)
      if(HAVE_GNU_READLINE)
          find_package_message(GNU_READLINE "Detected GNU Readline"
              "${HAVE_GNU_READLINE}")
      unset(CMAKE_REQUIRED_LIBRARIES)
      endif()
  endif()
  if(CURSES_FOUND)
    set(READLINE_LIBRARIES ${READLINE_LIBRARIES} ${CURSES_LIBRARIES})
    set(READLINE_INCLUDE_DIRS ${READLINE_INCLUDE_DIRS} ${CURSES_INCLUDE_DIRS})
  elseif(TERMCAP_FOUND)
    set(READLINE_LIBRARIES ${READLINE_LIBRARIES} ${TERMCAP_LIBRARIES})
    set(READLINE_INCLUDE_DIRS ${READLINE_INCLUDE_DIRS} ${TERMCAP_INCLUDE_DIRS})
  endif()
endif(READLINE_FOUND)

mark_as_advanced(READLINE_INCLUDE_DIRS READLINE_LIBRARIES)
