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
include_directories(${CMAKE_BINARY_DIR})

enable_testing()


include(GNUInstallDirs OPTIONAL)

if(NOT DEFINED CMAKE_INSTALL_BINDIR)
  set(CMAKE_INSTALL_BINDIR "bin")
endif()
if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR "lib")
endif()
if(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
  set(CMAKE_INSTALL_INCLUDEDIR "include")
endif()

######################################
##### Library setup              #####
######################################

if("${CMAKE_C_COMPILER}" MATCHES ".*lsb.*")
  set(LSB_COMPILER_FOUND TRUE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --lsb-shared-libs=tag:sndfile:rsvg-2 --lsb-besteffort -Wl,--allow-shlib-undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --lsb-shared-libs=tag:sndfile:rsvg-2 --lsb-besteffort -Wl,--allow-shlib-undefined")
endif()

find_package(GLIB)
find_package(GTK2)
find_package(RSVG2)
find_package(SNDFILE)
find_package(MPG123)
find_package(MPCDEC)
find_package(SPEEXDSP)
find_package(AVFORMAT)
find_package(TAGLIB)
find_package(GSTREAMER)
find_package(Qt4 COMPONENTS QtGui QtSvg)
find_library(M_LIBRARY m)


# set up install folders
set(FLAT_INSTALL FALSE CACHE BOOL "Install everything in one folder")
if(FLAT_INSTALL)
  set(FOLDER_DOC .)
  set(FOLDER_BIN .)
  set(FOLDER_LIB .)
  set(FOLDER_PLUGIN .)
else()
  set(FOLDER_BIN ${CMAKE_INSTALL_BINDIR})
  set(FOLDER_LIB ${CMAKE_INSTALL_LIBDIR})
  set(FOLDER_PLUGIN ${CMAKE_INSTALL_LIBDIR}/ebur128-loudness-tools)
  set(FOLDER_INCLUDE ${CMAKE_INSTALL_INCLUDEDIR})
endif()


set(USE_GLIB            ${GLIB_FOUND}      CACHE BOOL "Enable scanner support")
set(USE_GTK2            ${GTK2_FOUND}      CACHE BOOL "Enable GTK GUI scanner")
set(USE_RSVG2           ${RSVG2_FOUND}     CACHE BOOL "Needed for GTK GUI")
set(USE_SNDFILE         ${SNDFILE_FOUND}   CACHE BOOL "Enable libsndfile plugin")
set(USE_MPG123          ${MPG123_FOUND}    CACHE BOOL "Enable mpg123 plugin")
set(USE_MPCDEC          ${MPCDEC_FOUND}    CACHE BOOL "Enable mpcdec plugin")
set(USE_SPEEX_RESAMPLER ${SPEEXDSP_FOUND}  CACHE BOOL "Use speex resampler for true peak scanning")
set(USE_AVFORMAT        ${AVFORMAT_FOUND}  CACHE BOOL "Enable FFmpeg/Libav plugin")
set(USE_TAGLIB          ${TAGLIB_FOUND}    CACHE BOOL "Use taglib for ReplayGain compatible tagging")
set(USE_GSTREAMER       ${GSTREAMER_FOUND} CACHE BOOL "Enable GStreamer plugin")
set(USE_QT              ${QT_FOUND}        CACHE BOOL "Enable Qt4 GUI support")

set(PLUGIN_PATH "${CMAKE_INSTALL_PREFIX}/${FOLDER_PLUGIN}")


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/use_speex.h.in
               ${CMAKE_BINARY_DIR}/use_speex.h)
add_definitions(-DHAVE_CONFIG_USE_SPEEX_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/use_taglib.h.in
               ${CMAKE_BINARY_DIR}/use_taglib.h)
add_definitions(-DHAVE_CONFIG_USE_TAGLIB_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/use_sndfile.h.in
               ${CMAKE_BINARY_DIR}/use_sndfile.h)
add_definitions(-DHAVE_CONFIG_USE_SNDFILE_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/plugin_path.h.in
               ${CMAKE_BINARY_DIR}/plugin_path.h)
add_definitions(-DHAVE_CONFIG_PLUGIN_PATH_H)


######################################
##### Add libraries/executables  #####
######################################
macro(to_yes_no vars)
  foreach(var ${ARGV})
    if(${var})
      set(${var} yes)
    else()
      set(${var} no)
    endif()
  endforeach()
endmacro()

to_yes_no(GLIB_FOUND GTK2_FOUND RSVG2_FOUND SNDFILE_FOUND MPG123_FOUND MPCDEC_FOUND SPEEXDSP_FOUND AVFORMAT_FOUND TAGLIB_FOUND GSTREAMER_FOUND QT_FOUND)
to_yes_no(USE_GLIB USE_GTK2 USE_RSVG2 USE_SNDFILE USE_MPG123 USE_MPCDEC USE_SPEEX_RESAMPLER USE_AVFORMAT USE_TAGLIB USE_GSTREAMER USE_QT)

##### Print status
message(STATUS "status          found / use --")
message(STATUS "glib-2.0:       ${GLIB_FOUND}"     "     ${USE_GLIB}")
message(STATUS "gtk-2.0:        ${GTK2_FOUND}"     "     ${USE_GTK2}")
message(STATUS "rsvg2:          ${RSVG2_FOUND}"    "     ${USE_RSVG2}")
message(STATUS "libsndfile:     ${SNDFILE_FOUND}"  "     ${USE_SNDFILE}")
message(STATUS "libmpg123:      ${MPG123_FOUND}"   "     ${USE_MPG123}")
message(STATUS "libmpcdec:      ${MPCDEC_FOUND}"   "     ${USE_MPCDEC}")
message(STATUS "speexdsp:       ${SPEEXDSP_FOUND}" "     ${USE_SPEEX_RESAMPLER}")
message(STATUS "ffmpeg:         ${AVFORMAT_FOUND}" "     ${USE_AVFORMAT}")
message(STATUS "taglib:         ${TAGLIB_FOUND}"   "     ${USE_TAGLIB}")
message(STATUS "gstreamer:      ${GSTREAMER_FOUND}""     ${USE_GSTREAMER}")
message(STATUS "qt4:            ${QT_FOUND}"       "     ${USE_QT}")


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

if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND AVFORMAT_FOUND AND
   GLIB_FOUND AND TAGLIB_FOUND)
  set(CPACK_PACKAGE_VERSION_MAJOR "1")
  set(CPACK_PACKAGE_VERSION_MINOR "0")
  set(CPACK_PACKAGE_VERSION_PATCH "0")

  set(CPACK_GENERATOR ZIP TGZ)
  set(CPACK_SOURCE_GENERATOR ZIP TGZ)
  set(CPACK_STRIP_FILES TRUE)

  set(CPACK_SOURCE_IGNORE_FILES "/TODO;/build.*/;/.git/;/.gitignore;/.gitmodules;/mingw.*/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

  set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

  include(CPack)
endif()
