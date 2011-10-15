######################################
##### CMake environment setup    #####
######################################
project(libebur128)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${${PROJECT_NAME}_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})

include_directories(libebur128)
include_directories(libinputaudio)
include_directories(libfilewalk)
include_directories(scanner)

enable_testing()

######################################
##### Library setup              #####
######################################

if("${CMAKE_C_COMPILER}" MATCHES ".*lsb.*")
  set(LSB_COMPILER_FOUND TRUE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--allow-shlib-undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-shlib-undefined")
endif()

find_package(SNDFILE)
find_package(MPG123)
find_package(MPCDEC)
find_package(GLIB)
find_package(GTK2)
find_package(FFMPEG)
find_package(TAGLIB)
find_package(GStreamer)

if(SNDFILE_FOUND)
include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})
endif()
if(MPG123_FOUND)
include_directories(SYSTEM ${MPG123_INCLUDE_DIRS})
endif()
if(MPCDEC_FOUND)
include_directories(SYSTEM ${MPCDEC_INCLUDE_DIRS})
endif()
if(GLIB_FOUND)
include_directories(SYSTEM ${GLIB_INCLUDE_DIRS})
endif()
if(GTK2_FOUND)
include_directories(SYSTEM ${GTK2_INCLUDE_DIRS})
endif()
if(FFMPEG_FOUND)
include_directories(SYSTEM ${FFMPEG_INCLUDE_DIRS})
endif()
if(TAGLIB_FOUND)
include_directories(SYSTEM ${TAGLIB_INCLUDE_DIRS})
endif()
if(GSTREAMER_FOUND)
if(UNIX)
  set(GSTREAMER_INPUT_STATIC TRUE)
endif()
include_directories(SYSTEM ${GSTREAMER_INCLUDE_DIR})
foreach(arg ${GSTREAMER_DEFINITIONS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${arg}")
endforeach()
endif()

set(USE_TAGLIB ${TAGLIB_FOUND} CACHE BOOL "Use taglib for ReplayGain compatible tagging")
if(USE_TAGLIB)
  add_definitions(-DUSE_TAGLIB)
  if(WIN32)
    find_library(GCC_SJLJ_LIBRARY gcc_s_sjlj-1)
    find_library(LIBSTDCPP_LIBRARY stdc++-6)
    if(GCC_SJLJ_LIBRARY AND LIBSTDCPP_LIBRARY)
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
    else()
      message(FATAL_ERROR "Could not find gcc_s_sjlj-1.dll or stdc++-6.dll")
    endif()
  endif()
else()
  set(TAGLIB_FOUND FALSE)
endif()

######################################
##### Compiler setup             #####
######################################
if(NOT CMAKE_BUILD_TYPE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
endif()

include(CheckCCompilerFlag)

CHECK_C_COMPILER_FLAG("-std=c99" FLAG_STD_C99)

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


######################################
##### Add libraries/executables  #####
######################################

##### Print status
message(STATUS "status --")
message(STATUS "glib-2.0:       ${GLIB_FOUND}")
message(STATUS "libsndfile:     ${SNDFILE_FOUND}")
message(STATUS "libmpg123:      ${MPG123_FOUND}")
message(STATUS "libmpcdec:      ${MPCDEC_FOUND}")
message(STATUS "ffmpeg:         ${FFMPEG_FOUND}")
message(STATUS "taglib:         ${TAGLIB_FOUND}")

# set up install folders
set(FLAT_INSTALL FALSE CACHE BOOL "Install everything in one folder")
if(FLAT_INSTALL)
  set(FOLDER_DOC .)
  set(FOLDER_BIN .)
  set(FOLDER_PLUGIN .)
else()
  set(FOLDER_DOC share/doc)
  set(FOLDER_BIN bin)
  set(FOLDER_PLUGIN bin/r128)
endif()


set(EBUR128_LINK_AGAINST_SCANNER TRUE)
add_subdirectory(libebur128)
add_subdirectory(libinputaudio)
if(GLIB_FOUND)
  add_subdirectory(libfilewalk)
endif()
add_subdirectory(scanner)
add_subdirectory(test)


######################################
##### Install                    #####
######################################
install(FILES COPYING README.md DESTINATION ${FOLDER_DOC})
install(DIRECTORY doc/license DESTINATION ${FOLDER_DOC})

if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND FFMPEG_FOUND AND
   GLIB_FOUND AND TAGLIB_FOUND)
  set(CPACK_PACKAGE_VERSION_MAJOR "0")
  set(CPACK_PACKAGE_VERSION_MINOR "4")
  set(CPACK_PACKAGE_VERSION_PATCH "0")

  set(CPACK_GENERATOR ZIP TGZ)
  set(CPACK_SOURCE_GENERATOR ZIP TGZ)
  set(CPACK_STRIP_FILES TRUE)

  set(CPACK_SOURCE_IGNORE_FILES "/TODO;/build.*/;/.git/;/.gitignore;/mingw.*/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

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
