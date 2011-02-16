######################################
##### CMake environment setup    #####
######################################
project(libebur128)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${${PROJECT_NAME}_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})


######################################
##### Library setup              #####
######################################

include(FindPkgConfig)
if("${CMAKE_C_COMPILER}" MATCHES ".*lsb.*")
  set(LSB_COMPILER_FOUND TRUE)
  set(PKG_CONFIG_PREFIX "/opt/lsb/lib/pkgconfig/")
  set(PKG_CONFIG_SUFFIX ".pc")
endif()

##### glib
if(NOT WIN32)
  pkg_check_modules(GLIB "${PKG_CONFIG_PREFIX}glib-2.0${PKG_CONFIG_SUFFIX}")
  pkg_check_modules(GTHREAD "${PKG_CONFIG_PREFIX}gthread-2.0${PKG_CONFIG_SUFFIX}")
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
  pkg_check_modules(SNDFILE sndfile>=1.0.21)
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
if(NOT WIN32)
  pkg_check_modules(LIBAVCODEC libavcodec)
  pkg_check_modules(LIBAVFORMAT libavformat)
  if(LIBAVCODEC_FOUND AND LIBAVFORMAT_FOUND)
    include_directories(SYSTEM ${LIBAVCODEC_INCLUDE_DIRS})
    include_directories(SYSTEM ${LIBAVFORMAT_INCLUDE_DIRS})
    link_directories(${LIBAVCODEC_LIBRARY_DIRS})
    link_directories(${LIBAVFORMAT_LIBRARY_DIRS})
    set(FFMPEG_FOUND TRUE)
  endif()
else()
  find_library(LIBAVCODEC_LIBRARIES avcodec-52)
  find_library(LIBAVFORMAT_LIBRARIES avformat-52)
  find_library(LIBAVUTIL_LIBRARIES avutil-50)
  if(LIBAVCODEC_LIBRARIES AND LIBAVFORMAT_LIBRARIES AND LIBAVUTIL_LIBRARIES)
    set(FFMPEG_FOUND TRUE)
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
if(MPCDEC_INCLUDE_DIR)
  list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
  find_library(MPCDEC_LIBRARY NAMES mpcdec mpcdec_static)
  list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
  if(MPCDEC_LIBRARY)
    set(MPCDEC_FOUND TRUE)
  else()
    message(STATUS "Could not find the mpcdec library! Will not build mpcdec example.")
  endif()
else()
  message(STATUS "Could not find mpc include dir! Will not build mpcdec example.")
endif()

##### taglib
if(NOT WIN32)
  pkg_check_modules(TAGLIB taglib)
  if(TAGLIB_FOUND)
    include_directories(SYSTEM ${TAGLIB_INCLUDE_DIRS})
    link_directories(${TAGLIB_LIBRARY_DIRS})
  endif()
endif()


######################################
##### Compiler setup             #####
######################################
include("${CMAKE_MODULE_PATH}/CompilerFlags.txt")  # This sets VALID_FLAGS
CHECK_C_COMPILER_FLAG("-ansi" FLAG_ANSI)
if(FLAG_ANSI)
  set(CMAKE_C_FLAGS "-ansi")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${VALID_FLAGS}")

if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  CHECK_C_COMPILER_FLAG("-msse2 -mfpmath=sse" FLAG_SSE2_OPT)
  if(FLAG_SSE2_OPT)
    set(USE_SSE2 FALSE CACHE BOOL "Enable to build with SSE2")
    if(USE_SSE2)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2 -mfpmath=sse")
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
add_library(ebur128 src/ebur128.c)
if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()

if(GLIB_FOUND AND GTHREAD_FOUND AND INTL_FOUND)
  add_library(nproc src/nproc.c)
  if(SNDFILE_FOUND)
    add_library(sndfile_input src/input_sndfile.c)
    target_link_libraries(sndfile_input ${SNDFILE_LIBRARIES})

    add_executable(r128-minimal-example src/minimal_example.c)
    target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})

    add_executable(r128-sndfile src/scanner.c)
    target_link_libraries(r128-sndfile ebur128 nproc sndfile_input ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})

    add_executable(r128-test-library src/tests.c)
    target_link_libraries(r128-test-library ebur128 ${SNDFILE_LIBRARIES})
  endif()

  if(MPG123_FOUND)
    add_library(mpg123_input src/input_mpg123.c)
    target_link_libraries(mpg123_input ${MPG123_LIBRARIES})

    add_executable(r128-mpg123 src/scanner.c)
    target_link_libraries(r128-mpg123 ebur128 nproc mpg123_input ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})
  endif()

  if(MPCDEC_FOUND)
    add_library(musepack_input src/input_mpcdec.c)
    target_link_libraries(musepack_input ${MPCDEC_LIBRARY})

    add_executable(r128-musepack src/scanner.c)
    target_link_libraries(r128-musepack ebur128 nproc musepack_input ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})
  endif()

  if(FLAG_STD_C99 AND FFMPEG_FOUND)
    if(LSB_COMPILER_FOUND)
      set(R128_FFMPEG r128-ffmpeg-${FFMPEG_VERSION})
      set(FFMPEG_INPUT ffmpeg_input_${FFMPEG_VERSION})
    else()
      set(R128_FFMPEG r128-ffmpeg)
      set(FFMPEG_INPUT ffmpeg_input)
    endif()
    add_library(${FFMPEG_INPUT} src/input_ffmpeg.c)
    target_link_libraries(${FFMPEG_INPUT} ${LIBAVCODEC_LIBRARIES} ${LIBAVFORMAT_LIBRARIES} ${LIBAVUTIL_LIBRARIES})
    set_target_properties(${FFMPEG_INPUT} PROPERTIES COMPILE_FLAGS "-std=c99")

    add_executable(${R128_FFMPEG} src/scanner.c)
    target_link_libraries(${R128_FFMPEG} ebur128 nproc ${FFMPEG_INPUT} ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})
  endif()

  if(TAGLIB_FOUND)
    add_executable(rgtag src/rgtag.cpp)
    target_link_libraries(rgtag ${TAGLIB_LIBRARIES})
  endif()
endif()


######################################
##### Install                    #####
######################################
message(STATUS "'make package' status:")
message(STATUS "glib-2.0:       ${GLIB_FOUND}")
message(STATUS "gthread-2.0:    ${GTHREAD_FOUND}")
message(STATUS "intl (gettext): ${INTL_FOUND}")
message(STATUS "libsndfile:     ${SNDFILE_FOUND}")
message(STATUS "libmpg123:      ${MPG123_FOUND}")
message(STATUS "libmpcdec:      ${MPCDEC_FOUND}")
message(STATUS "ffmpeg:         ${FFMPEG_FOUND}")
if(GLIB_FOUND AND GTHREAD_FOUND AND INTL_FOUND AND SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND FFMPEG_FOUND)
  install(TARGETS r128-sndfile ${R128_FFMPEG} r128-mpg123 r128-musepack
          RUNTIME DESTINATION .
          ARCHIVE DESTINATION lib)
  install(FILES LICENSE README.md DESTINATION .)
  if(LSB_COMPILER_FOUND)
    install(PROGRAMS ${EXECUTABLE_OUTPUT_PATH}/r128-ffmpeg-0.5.3
                     ${EXECUTABLE_OUTPUT_PATH}/r128-ffmpeg-0.6.1 DESTINATION .)
  endif()
  install(PROGRAMS src/rgtag.py src/r128-tag.py DESTINATION .)
  install(DIRECTORY doc/license DESTINATION .)
  if(WIN32)
    install(FILES
            ${LIBAVCODEC_LIBRARIES}
            ${LIBAVFORMAT_LIBRARIES}
            ${LIBAVUTIL_LIBRARIES}
            ${MPG123_LIBRARIES}
            ${SNDFILE_LIBRARIES}
            ${INTL_LIBRARIES}
            ${GLIB_LIBRARIES}
            ${GTHREAD_LIBRARIES}
            DESTINATION .)
  endif()

  set(CPACK_PACKAGE_VERSION_MAJOR "0")
  set(CPACK_PACKAGE_VERSION_MINOR "1")
  set(CPACK_PACKAGE_VERSION_PATCH "12")

  set(CPACK_GENERATOR ZIP TGZ)
  set(CPACK_SOURCE_GENERATOR ZIP TGZ)

  set(INCLUDE_BLACKLIST "include/mpc/;include/ffmpeg-0.5.3/;include/ffmpeg-0.6.1/;include/mpg123.h;include/sndfile.h")
  set(BUILDSCRIPT "build.sh")
  set(CPACK_SOURCE_IGNORE_FILES "${BUILDSCRIPT};build/;build32/;.git/;.gitignore;${INCLUDE_BLACKLIST};lib/;lib64/;mingw/;mingw64/;mingw-include/;lsb/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

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
