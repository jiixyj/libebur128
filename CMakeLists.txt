######################################
##### CMake environment setup    #####
######################################
project(ebur128-loudness-tools)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories(ebur128)
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
find_package(RSVG2)
find_package(SPEEXDSP)
find_package(FFMPEG)
find_package(TAGLIB)
find_package(GSTREAMER)
find_package(Qt4 COMPONENTS QtGui QtSvg)

set(EBUR128_USE_SPEEX_RESAMPLER ${SPEEXDSP_FOUND})

if(GSTREAMER_FOUND AND UNIX)
  set(GSTREAMER_INPUT_STATIC TRUE)
endif()

set(USE_TAGLIB ${TAGLIB_FOUND} CACHE BOOL "Use taglib for ReplayGain compatible tagging")
if(NOT USE_TAGLIB)
  set(TAGLIB_FOUND FALSE)
endif()


include_directories(${CMAKE_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/use_speex.h.in
               ${CMAKE_BINARY_DIR}/use_speex.h)
add_definitions(-DHAVE_CONFIG_USE_SPEEX_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/use_taglib.h.in
               ${CMAKE_BINARY_DIR}/use_taglib.h)
add_definitions(-DHAVE_CONFIG_USE_TAGLIB_H)


######################################
##### Add libraries/executables  #####
######################################

##### Print status
message(STATUS "status --")
message(STATUS "glib-2.0:       ${GLIB_FOUND}")
message(STATUS "gtk-2.0:        ${GTK2_FOUND}")
message(STATUS "libsndfile:     ${SNDFILE_FOUND}")
message(STATUS "libmpg123:      ${MPG123_FOUND}")
message(STATUS "libmpcdec:      ${MPCDEC_FOUND}")
message(STATUS "speexdsp:       ${SPEEXDSP_FOUND}")
message(STATUS "ffmpeg:         ${FFMPEG_FOUND}")
message(STATUS "taglib:         ${TAGLIB_FOUND}")
message(STATUS "gstreamer:      ${GSTREAMER_FOUND}")
message(STATUS "qt4:            ${QT_FOUND}")

# set up install folders
set(FLAT_INSTALL FALSE CACHE BOOL "Install everything in one folder")
if(FLAT_INSTALL)
  set(FOLDER_DOC .)
  set(FOLDER_BIN .)
  set(FOLDER_LIB .)
  set(FOLDER_PLUGIN .)
else()
  set(FOLDER_BIN bin)
  set(FOLDER_LIB lib)
  set(FOLDER_PLUGIN lib/ebur128-loudness-tools)
  set(FOLDER_INCLUDE include)
endif()


add_subdirectory(ebur128)
add_subdirectory(scanner)
add_subdirectory(test)


######################################
##### Install                    #####
######################################
if(FOLDER_DOC)
    install(FILES COPYING README.md DESTINATION ${FOLDER_DOC})
    install(DIRECTORY doc/license DESTINATION ${FOLDER_DOC})
endif()

if(WIN32)
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(DLL_FOLDER dlls-64)
  else()
    set(DLL_FOLDER dlls-32)
  endif()
  install(DIRECTORY ${DLL_FOLDER}/ DESTINATION ${FOLDER_BIN}
          FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                           GROUP_EXECUTE GROUP_READ
                           WORLD_EXECUTE WORLD_READ)
endif()

if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND FFMPEG_FOUND AND
   GLIB_FOUND AND TAGLIB_FOUND)
  set(CPACK_PACKAGE_VERSION_MAJOR "0")
  set(CPACK_PACKAGE_VERSION_MINOR "5")
  set(CPACK_PACKAGE_VERSION_PATCH "0")

  set(CPACK_GENERATOR ZIP TGZ)
  set(CPACK_SOURCE_GENERATOR ZIP TGZ)
  set(CPACK_STRIP_FILES TRUE)

  set(CPACK_SOURCE_IGNORE_FILES "/TODO;/build.*/;/.git/;/.gitignore;/.gitmodules;/mingw.*/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

  if(WIN32 AND ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(CPACK_SYSTEM_NAME "win64")
  elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" AND ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(CPACK_SYSTEM_NAME "Linux64")
  endif()

  include(CPack)
endif()
