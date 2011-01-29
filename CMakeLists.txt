######################################
##### CMake environment setup    #####
######################################
project(libebur128 C)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${${PROJECT_NAME}_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})

set(FFMPEG_VERSION "0.6.1" CACHE STRING "FFmpeg version to link to")

if(NOT WIN32)
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    list(APPEND LINK_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/lib64)
    list(APPEND LINK_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/lib64/ffmpeg-${FFMPEG_VERSION})
  elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    list(APPEND LINK_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/lib)
    list(APPEND LINK_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/lib/ffmpeg-${FFMPEG_VERSION})
  endif()
else()
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    list(APPEND LINK_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/mingw-lib64)
  elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
    list(APPEND LINK_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/mingw-lib)
  endif()
endif()
link_directories(${LINK_DIRECTORIES})

if(IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include)
  list(APPEND INCLUDE_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/include)
endif()
if(IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include/ffmpeg-${FFMPEG_VERSION})
  list(APPEND INCLUDE_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/include/ffmpeg-${FFMPEG_VERSION})
endif()
if(WIN32 AND IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/mingw-include)
  list(APPEND INCLUDE_DIRECTORIES ${${PROJECT_NAME}_SOURCE_DIR}/mingw-include)
endif()
include_directories(${INCLUDE_DIRECTORIES})


######################################
##### Library setup              #####
######################################

include(FindPkgConfig)
if("${CMAKE_C_COMPILER}" MATCHES ".*lsb.*")
  set(LSB_COMPILER_FOUND TRUE)
endif()

##### glib
if(NOT WIN32)
  pkg_check_modules(GLIB glib-2.0)
  pkg_check_modules(GTHREAD gthread-2.0)
  include_directories(SYSTEM ${GLIB_INCLUDEDIR})
  include_directories(SYSTEM ${GTHREAD_INCLUDEDIR})
  link_directories(${GLIB_LIBRARY_DIRS})
  link_directories(${GTHREAD_LIBRARY_DIRS})
endif()

##### libsndfile
if(NOT WIN32 AND NOT LSB_COMPILER_FOUND)
  pkg_check_modules(SNDFILE sndfile>=1.0.21)
  include_directories(SYSTEM ${SNDFILE_INCLUDEDIR})
  link_directories(${SNDFILE_LIBRARY_DIRS})
elseif(WIN32)
  find_library(SNDFILE_LIBRARIES sndfile-1
               PATHS ${LINK_DIRECTORIES} CMAKE_FIND_ROOT_PATH_BOTH)
  set(SNDFILE_FOUND ${SNDFILE_LIBRARIES})
elseif(LSB_COMPILER_FOUND)
  set(SNDFILE_LIBRARIES sndfile)
  set(SNDFILE_FOUND TRUE)
endif()

##### ffmpeg
if(NOT WIN32 AND NOT LSB_COMPILER_FOUND)
  pkg_check_modules(LIBAVCODEC libavcodec)
  pkg_check_modules(LIBAVFORMAT libavformat)
  include_directories(SYSTEM ${LIBAVCODEC_INCLUDEDIR})
  include_directories(SYSTEM ${LIBAVFORMAT_INCLUDEDIR})
  link_directories(${LIBAVCODEC_LIBRARY_DIRS})
  link_directories(${LIBAVFORMAT_LIBRARY_DIRS})
elseif(WIN32)
  find_library(LIBAVCODEC_LIBRARIES avcodec-52
               PATHS ${LINK_DIRECTORIES} CMAKE_FIND_ROOT_PATH_BOTH)
  find_library(LIBAVFORMAT_LIBRARIES avformat-52
               PATHS ${LINK_DIRECTORIES} CMAKE_FIND_ROOT_PATH_BOTH)
  find_library(LIBAVUTIL_LIBRARIES avutil-50
               PATHS ${LINK_DIRECTORIES} CMAKE_FIND_ROOT_PATH_BOTH)
  set(LIBAVCODEC_FOUND ${LIBAVCODEC_LIBRARIES})
  set(LIBAVFORMAT_FOUND ${LIBAVFORMAT_LIBRARIES})
  set(LIBAVUTIL_FOUND ${LIBAVCODEC_LIBRARIES})
elseif(LSB_COMPILER_FOUND)
  set(LIBAVCODEC_LIBRARIES avcodec)
  set(LIBAVFORMAT_LIBRARIES avformat)
  set(LIBAVUTIL_LIBRARIES "")
  set(LIBAVCODEC_FOUND TRUE)
  set(LIBAVFORMAT_FOUND TRUE)
  set(LIBAVUTIL_FOUND TRUE)
endif()
if(LIBAVCODEC_FOUND AND LIBAVFORMAT_FOUND AND (NOT WIN32 OR LIBAVUTIL_FOUND))
  set(FFMPEG_FOUND TRUE)
endif()

##### mpg123
if(NOT WIN32 AND NOT LSB_COMPILER_FOUND)
  pkg_check_modules(MPG123 libmpg123)
  include_directories(SYSTEM ${MPG123_INCLUDEDIR})
  link_directories(${MPG123_LIBRARY_DIRS})
elseif(WIN32)
  find_library(MPG123_LIBRARIES mpg123-0
               PATHS ${LINK_DIRECTORIES} CMAKE_FIND_ROOT_PATH_BOTH)
  set(MPG123_FOUND ${MPG123_LIBRARIES})
elseif(LSB_COMPILER_FOUND)
  set(MPG123_LIBRARIES mpg123)
  set(MPG123_FOUND TRUE)
endif()

##### mpddec
if(IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include/mpc)
  find_library(MPCDEC_LIBRARY mpcdec_static
               PATHS ${LINK_DIRECTORIES} CMAKE_FIND_ROOT_PATH_BOTH)
  if(MPCDEC_LIBRARY)
    set(MPCDEC_FOUND TRUE)
  else()
    set(MPCDEC_FOUND FALSE)
    message(STATUS "Could not find the mpcdec library! Will not build mpcdec example.")
  endif()
else()
  set(MPCDEC_FOUND FALSE)
  message(STATUS "Could not find mpc include dir! Will not build mpcdec example.")
endif()


######################################
##### Compiler setup             #####
######################################
include("${CMAKE_MODULE_PATH}/CompilerFlags.txt")  # This sets VALID_FLAGS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi ${VALID_FLAGS}")
if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2 -mfpmath=sse")
endif()


######################################
##### Add libraries/executables  #####
######################################
add_library(ebur128 src/ebur128.c)
if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()

if(SNDFILE_FOUND)
  add_executable(r128-minimal-example src/minimal_example.c)
  target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})

  add_executable(r128-sndfile src/sndfile_example.c)
  target_link_libraries(r128-sndfile ebur128 ${SNDFILE_LIBRARIES} ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})

  add_executable(r128-test-library src/tests.c)
  target_link_libraries(r128-test-library ebur128 ${SNDFILE_LIBRARIES})
  set_target_properties(r128-test-library PROPERTIES COMPILE_FLAGS "-Wno-float-equal")
endif()

if(MPG123_FOUND)
  add_executable(r128-mpg123 src/mpg123_example.c)
  target_link_libraries(r128-mpg123 ebur128 ${MPG123_LIBRARIES})
endif()

if(MPCDEC_FOUND)
  add_executable(r128-musepack src/mpcdec_example.c)
  target_link_libraries(r128-musepack ${MPCDEC_LIBRARY} ebur128)
  set_target_properties(r128-musepack PROPERTIES COMPILE_FLAGS "-std=c99")
endif()

if(FFMPEG_FOUND)
  if(LSB_COMPILER_FOUND)
    set(R128_FFMPEG r128-ffmpeg-${FFMPEG_VERSION})
  else()
    set(R128_FFMPEG r128-ffmpeg)
  endif()
  add_executable(${R128_FFMPEG} src/ffmpeg_example.c)
  target_link_libraries(${R128_FFMPEG} ebur128 ${LIBAVCODEC_LIBRARIES} ${LIBAVFORMAT_LIBRARIES} ${LIBAVUTIL_LIBRARIES})
  set_target_properties(${R128_FFMPEG} PROPERTIES COMPILE_FLAGS "-std=c99")
endif()


######################################
##### Install                    #####
######################################
if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND FFMPEG_FOUND)
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
  if(WIN32 AND CMAKE_C_COMPILER MATCHES ".*w64.*")
    install(FILES
            mingw-lib64/avcodec-52.dll
            mingw-lib64/avformat-52.dll
            mingw-lib64/avutil-50.dll
            mingw-lib64/libmpg123-0.dll
            mingw-lib64/libsndfile-1.dll
            DESTINATION .)
  elseif(WIN32)
    install(FILES
            mingw-lib/avcodec-52.dll
            mingw-lib/avformat-52.dll
            mingw-lib/avutil-50.dll
            mingw-lib/libmpg123-0.dll
            mingw-lib/libsndfile-1.dll
            DESTINATION .)
  endif()

  set(CPACK_PACKAGE_VERSION_MAJOR "0")
  set(CPACK_PACKAGE_VERSION_MINOR "1")
  set(CPACK_PACKAGE_VERSION_PATCH "9")

  set(CPACK_BINARY_ZIP ON)
  set(CPACK_BINARY_NSIS OFF)
  set(CPACK_SOURCE_ZIP ON)
  set(INCLUDE_BLACKLIST "include/mpc/;include/ffmpeg-0.5.3/;include/ffmpeg-0.6.1/;include/mpg123.h;include/sndfile.h")
  set(BUILDSCRIPT "build.sh")
  set(CPACK_SOURCE_IGNORE_FILES "${BUILDSCRIPT};build/;build32/;.git/;.gitignore;${INCLUDE_BLACKLIST};lib/;lib64/;mingw/;mingw64/;mingw-include/;lsb/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

  if(CMAKE_C_COMPILER MATCHES ".*w64.*")
    set(CPACK_SYSTEM_NAME "win64")
  elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" AND ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(CPACK_SYSTEM_NAME "Linux64")
  endif()

  include(CPack)
endif()
