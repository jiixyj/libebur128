######################################
##### CMake environment setup    #####
######################################
project(libebur128)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${${PROJECT_NAME}_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})
include_directories(${${PROJECT_NAME}_SOURCE_DIR}/src)


######################################
##### Library setup              #####
######################################

include(FindPkgConfig)
if("${CMAKE_C_COMPILER}" MATCHES ".*lsb.*")
  set(LSB_COMPILER_FOUND TRUE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--allow-shlib-undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-shlib-undefined")
endif()

##### glib
if(NOT WIN32)
  pkg_check_modules(GLIB glib-2.0)
  pkg_check_modules(GTHREAD gthread-2.0)
  if(GLIB_FOUND AND GTHREAD_FOUND)
    message(STATUS "${GLIB_INCLUDE_DIRS} ${GTHREAD_LIBRARY_DIRS}")
    include_directories(SYSTEM ${GLIB_INCLUDE_DIRS})
    include_directories(SYSTEM ${GTHREAD_INCLUDE_DIRS})
    link_directories(${GLIB_LIBRARY_DIRS})
    link_directories(${GTHREAD_LIBRARY_DIRS})
    set(INTL_FOUND TRUE)
  endif()
else()
  find_library(GLIB_LIBRARIES glib-2.0-0)
  find_library(GTHREAD_LIBRARIES gthread-2.0-0)
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    find_library(INTL_LIBRARIES intl)
  elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    find_library(INTL_LIBRARIES intl-8)
  endif()
  if(INTL_LIBRARIES)
    set(INTL_FOUND TRUE)
  endif()
  find_path(GLIB_INCLUDE_DIR glib.h PATH_SUFFIXES glib-2.0)
  find_path(GLIB_LIB_INCLUDE_DIR glibconfig.h PATH_SUFFIXES ../lib/glib-2.0/include)
  if(GLIB_LIBRARIES AND GTHREAD_LIBRARIES AND GLIB_INCLUDE_DIR AND GLIB_LIB_INCLUDE_DIR)
    include_directories(SYSTEM ${GLIB_INCLUDE_DIR})
    include_directories(SYSTEM ${GLIB_LIB_INCLUDE_DIR})
    set(GLIB_FOUND TRUE)
    set(GTHREAD_FOUND TRUE)
  endif()
endif()

##### libsndfile
if(NOT WIN32)
  pkg_check_modules(SNDFILE sndfile>=1.0.18)
  if(SNDFILE_FOUND)
    include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})
    link_directories(${SNDFILE_LIBRARY_DIRS})
  endif()
else()
  find_library(SNDFILE_LIBRARIES sndfile-1)
  find_path(SNDFILE_INCLUDE_DIR sndfile.h)
  if(SNDFILE_LIBRARIES AND SNDFILE_INCLUDE_DIR)
    include_directories(SYSTEM ${SNDFILE_INCLUDE_DIR})
    set(SNDFILE_FOUND TRUE)
  endif()
endif()

##### ffmpeg
if(LSB_COMPILER_FOUND)
  if(NOT FFMPEG_VERSION)
    set(FFMPEG_VERSION "0.5.2")
  endif()
endif()
if("${FFMPEG_VERSION}" STREQUAL "0.6.1")
  message(STATUS "Hack: building with FFmpeg 0.6.1")
  message(STATUS "incl: ${FFMPEG_NEW_INC_DIR}")
  message(STATUS "libs: ${FFMPEG_NEW_LIB_DIR}")
  include_directories(SYSTEM ${FFMPEG_NEW_INC_DIR})
  link_directories(${FFMPEG_NEW_LIB_DIR})
elseif(NOT WIN32)
  pkg_check_modules(LIBAVCODEC libavcodec)
  pkg_check_modules(LIBAVFORMAT libavformat)
  pkg_check_modules(LIBAVUTIL libavutil)
  if(LIBAVCODEC_FOUND AND LIBAVFORMAT_FOUND AND LIBAVUTIL_FOUND)
    include_directories(SYSTEM ${LIBAVCODEC_INCLUDE_DIRS})
    include_directories(SYSTEM ${LIBAVFORMAT_INCLUDE_DIRS})
    include_directories(SYSTEM ${LIBAVUTIL_INCLUDE_DIRS})
    link_directories(${LIBAVCODEC_LIBRARY_DIRS})
    link_directories(${LIBAVFORMAT_LIBRARY_DIRS})
    link_directories(${LIBAVUTIL_LIBRARY_DIRS})
  endif()
else()
  find_library(LIBAVCODEC_LIBRARIES avcodec-52)
  find_library(LIBAVFORMAT_LIBRARIES avformat-52)
  find_library(LIBAVUTIL_LIBRARIES avutil-50)
  if(LIBAVCODEC_LIBRARIES)
    set(LIBAVCODEC_FOUND TRUE)
  endif()
  if(LIBAVFORMAT_LIBRARIES)
    set(LIBAVFORMAT_FOUND TRUE)
  endif()
  if(LIBAVUTIL_LIBRARIES)
    set(LIBAVUTIL_FOUND TRUE)
  endif()
endif()

##### mpg123
if(NOT WIN32)
  pkg_check_modules(MPG123 libmpg123)
  if(MPG123_FOUND)
    include_directories(SYSTEM ${MPG123_INCLUDE_DIRS})
    link_directories(${MPG123_LIBRARY_DIRS})
  endif()
else()
  find_library(MPG123_LIBRARIES mpg123-0)
  if(MPG123_LIBRARIES)
    set(MPG123_FOUND TRUE)
  endif()
endif()

##### mpddec
find_path(MPCDEC_INCLUDE_DIR mpcdec.h PATH_SUFFIXES mpc)
list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
find_library(MPCDEC_LIBRARY NAMES mpcdec mpcdec_static)
list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
if(MPCDEC_INCLUDE_DIR AND MPCDEC_LIBRARY)
  set(MPCDEC_FOUND TRUE)
endif()

##### taglib
set(USE_TAGLIB TRUE CACHE BOOL "Use taglib for ReplayGain compatible tagging")
if(USE_TAGLIB)
  if(NOT WIN32)
    pkg_check_modules(TAGLIB taglib)
    if(TAGLIB_FOUND)
      include_directories(SYSTEM ${TAGLIB_INCLUDE_DIRS})
      link_directories(${TAGLIB_LIBRARY_DIRS})
    endif()
  else()
    find_library(TAGLIB_LIBRARIES tag)
    find_library(GCC_SJLJ_LIBRARY gcc_s_sjlj-1)
    find_library(LIBSTDCPP_LIBRARY stdc++-6)
    find_path(TAGLIB_INCLUDE_DIR taglib.h PATH_SUFFIXES taglib)
    if(TAGLIB_LIBRARIES AND TAGLIB_INCLUDE_DIR AND
       GCC_SJLJ_LIBRARY AND LIBSTDCPP_LIBRARY)
      get_filename_component(STDCPP_NAME ${LIBSTDCPP_LIBRARY} NAME)
      get_filename_component(GCC_SJLJ_NAME ${GCC_SJLJ_LIBRARY} NAME)
      execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                      ${GCC_SJLJ_LIBRARY}
                      ${CMAKE_CURRENT_BINARY_DIR}/${GCC_SJLJ_NAME})
      execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                      ${LIBSTDCPP_LIBRARY}
                      ${CMAKE_CURRENT_BINARY_DIR}/${STDCPP_NAME})
      set(GCC_SJLJ ${CMAKE_CURRENT_BINARY_DIR}/${GCC_SJLJ_NAME})
      set(STDCPP ${CMAKE_CURRENT_BINARY_DIR}/${STDCPP_NAME})
      execute_process(COMMAND ${CMAKE_STRIP} ${GCC_SJLJ})
      execute_process(COMMAND ${CMAKE_STRIP} ${STDCPP})
      include_directories(SYSTEM ${TAGLIB_INCLUDE_DIR})
      set(TAGLIB_FOUND TRUE)
    endif()
  endif()
  if(TAGLIB_FOUND)
    add_definitions(-DUSE_TAGLIB)
  endif()
endif()


######################################
##### Compiler setup             #####
######################################
include("${CMAKE_MODULE_PATH}/CompilerFlags.txt")  # This sets VALID_FLAGS
if(NOT WIN32)
  CHECK_C_COMPILER_FLAG("-ansi" FLAG_ANSI)
  if(FLAG_ANSI)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi")
  endif()
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${VALID_FLAGS}")

if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  CHECK_C_COMPILER_FLAG("-msse2 -mfpmath=sse" FLAG_SSE2_OPT)
  if(FLAG_SSE2_OPT)
    set(USE_SSE2 FALSE CACHE BOOL "Enable SSE2 optimizations")
    if(USE_SSE2)
      set(CMAKE_C_FLAGS_R128 "-msse2 -mfpmath=sse")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_R128}")
    endif()
  endif()
endif()

CHECK_C_COMPILER_FLAG("-isystem /usr/include" FLAG_ISYSTEM)
if(FLAG_ISYSTEM)
  set(CMAKE_INCLUDE_SYSTEM_FLAG_C "-isystem ")
endif()

CHECK_C_COMPILER_FLAG("-std=c99" FLAG_STD_C99)


######################################
##### Add libraries/executables  #####
######################################

##### Print status
macro(_r128_found_var_to_string val)
  if(${val})
    set(${val} TRUE)
  else()
    set(${val} FALSE)
  endif()
endmacro()
_r128_found_var_to_string(GLIB_FOUND)
_r128_found_var_to_string(GTHREAD_FOUND)
_r128_found_var_to_string(INTL_FOUND)
_r128_found_var_to_string(SNDFILE_FOUND)
_r128_found_var_to_string(MPG123_FOUND)
_r128_found_var_to_string(MPCDEC_FOUND)
_r128_found_var_to_string(LIBAVFORMAT_FOUND)
_r128_found_var_to_string(LIBAVCODEC_FOUND)
_r128_found_var_to_string(LIBAVUTIL_FOUND)
_r128_found_var_to_string(TAGLIB_FOUND)
message(STATUS "status:")
message(STATUS "glib-2.0:       ${GLIB_FOUND}")
message(STATUS "gthread-2.0:    ${GTHREAD_FOUND}")
if(WIN32)
  message(STATUS "intl (gettext): ${INTL_FOUND}")
endif()
message(STATUS "libsndfile:     ${SNDFILE_FOUND}")
message(STATUS "libmpg123:      ${MPG123_FOUND}")
message(STATUS "libmpcdec:      ${MPCDEC_FOUND}")
message(STATUS "libavformat:    ${LIBAVFORMAT_FOUND}")
message(STATUS "libavcodec:     ${LIBAVCODEC_FOUND}")
message(STATUS "libavutil:      ${LIBAVUTIL_FOUND}")
message(STATUS "taglib:         ${TAGLIB_FOUND}")

add_subdirectory(src)


######################################
##### Install                    #####
######################################
if(INSTALL_READY)
  install(FILES COPYING README DESTINATION .)
  install(DIRECTORY doc/license DESTINATION .)

  set(CPACK_PACKAGE_VERSION_MAJOR "0")
  set(CPACK_PACKAGE_VERSION_MINOR "3")
  set(CPACK_PACKAGE_VERSION_PATCH "2")

  set(CPACK_GENERATOR ZIP TGZ)
  set(CPACK_SOURCE_GENERATOR ZIP TGZ)
  set(CPACK_STRIP_FILES TRUE)

  set(CPACK_SOURCE_IGNORE_FILES "/build.*/;/.git/;/.gitignore;/mingw.*/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

  if(WIN32 AND ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(CPACK_SYSTEM_NAME "win64")
  elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" AND ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(CPACK_SYSTEM_NAME "Linux64")
  elseif(WIN32 AND USE_SSE2)
    set(CPACK_SYSTEM_NAME "win32-sse2")
  elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" AND USE_SSE2)
    set(CPACK_SYSTEM_NAME "Linux-sse2")
  endif()

  include(CPack)
endif()
