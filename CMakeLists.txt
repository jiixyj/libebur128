######################################
##### CMake environment setup    #####
######################################
project(ebur128-loudness-tools)
cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_subdirectory(ebur128)
add_subdirectory(scanner)

include(utils)


set(ENABLE_INTERNAL_QUEUE_H OFF CACHE BOOL "Use own queue.h")
set(DISABLE_SPEEXDSP        OFF CACHE BOOL "Don't build with speexdsp")
set(DISABLE_GLIB20          OFF CACHE BOOL "Don't build with glib")
set(DISABLE_SNDFILE         OFF CACHE BOOL "Don't build with sndfile")
set(DISABLE_TAGLIB          OFF CACHE BOOL "Don't build with taglib")
set(DISABLE_MPG123          OFF CACHE BOOL "Don't build with mpg123")
set(DISABLE_MPCDEC          OFF CACHE BOOL "Don't build with mpcdec")
set(DISABLE_GSTREAMER       OFF CACHE BOOL "Don't build with gstreamer")
set(DISABLE_FFMPEG          OFF CACHE BOOL "Don't build with ffmpeg")
set(DISABLE_RSVG2           OFF CACHE BOOL "Don't build with rsvg2")
set(DISABLE_GTK2            OFF CACHE BOOL "Don't build with gtk2")
set(DISABLE_QT4             OFF CACHE BOOL "Don't build with qt4")

to_yes_no(SUMMARY_HAS_QUEUE SUMMARY_SPEEXDSP_FOUND SUMMARY_GLIB20_FOUND
          SUMMARY_GTHREAD20_FOUND SUMMARY_SNDFILE_FOUND SUMMARY_TAGLIB_FOUND
          SUMMARY_MPG123_FOUND SUMMARY_MPCDEC_FOUND SUMMARY_GSTREAMER_FOUND
          SUMMARY_FFMPEG_FOUND SUMMARY_RSVG2_FOUND SUMMARY_GTK2_FOUND
          SUMMARY_QT4_FOUND)
to_yes_no(DISABLE_SPEEXDSP DISABLE_GLIB20 DISABLE_SNDFILE DISABLE_TAGLIB
          DISABLE_MPG123 DISABLE_MPCDEC DISABLE_GSTREAMER DISABLE_FFMPEG
          DISABLE_RSVG2 DISABLE_GTK2 DISABLE_QT4)

if(ENABLE_INTERNAL_QUEUE_H)
    set(USE_QUEUE "using own copy of queue.h")
else()
    set(USE_QUEUE "using system copy of queue.h")
endif()

##### Print status
message(STATUS "status          found / disabled --")
message(STATUS "queue.h:        ${SUMMARY_HAS_QUEUE}"        "     ${USE_QUEUE}")
message(STATUS "speexdsp:       ${SUMMARY_SPEEXDSP_FOUND}"   "     ${DISABLE_SPEEXDSP}")
message(STATUS "glib-2.0:       ${SUMMARY_GLIB20_FOUND}"     "     ${DISABLE_GLIB20}")
message(STATUS "gthread-2.0:    ${SUMMARY_GTHREAD20_FOUND}"  "     ${DISABLE_GLIB20}")
message(STATUS "sndfile:        ${SUMMARY_SNDFILE_FOUND}"    "     ${DISABLE_SNDFILE}")
message(STATUS "taglib:         ${SUMMARY_TAGLIB_FOUND}"     "     ${DISABLE_TAGLIB}")
message(STATUS "mpg123:         ${SUMMARY_MPG123_FOUND}"     "     ${DISABLE_MPG123}")
message(STATUS "mpcdec:         ${SUMMARY_MPCDEC_FOUND}"     "     ${DISABLE_MPCDEC}")
message(STATUS "gstreamer:      ${SUMMARY_GSTREAMER_FOUND}"  "     ${DISABLE_GSTREAMER}")
message(STATUS "ffmpeg:         ${SUMMARY_FFMPEG_FOUND}"     "     ${DISABLE_FFMPEG}")
message(STATUS "rsvg2:          ${SUMMARY_RSVG2_FOUND}"      "     ${DISABLE_RSVG2}")
message(STATUS "gtk2:           ${SUMMARY_GTK2_FOUND}"       "     ${DISABLE_GTK2}")
message(STATUS "qt4:            ${SUMMARY_QT4_FOUND}"        "     ${DISABLE_QT4}")



if(NOT SUMMARY_HAS_QUEUE AND NOT ENABLE_INTERNAL_QUEUE_H)
    message(FATAL_ERROR "queue.h not found, please set ENABLE_INTERNAL_QUEUE_H to ON")
endif()

# if(NOT SUMMARY_SPEEXDSP_FOUND AND NOT DISABLE_SPEEXDSP)
#     message(WARNING "speexdsp not found, please define SPEEXDSP_FOUND, SPEEXDSP_CFLAGS and SPEEXDSP_LDFLAGS manually if you want speex resampling. Alternatively, set DISABLE_SPEEXDSP to ON to suppress this warning")
# endif()



# to_yes_no(GTK2_FOUND RSVG2_FOUND SNDFILE_FOUND MPG123_FOUND MPCDEC_FOUND SPEEXDSP_FOUND AVFORMAT_FOUND TAGLIB_FOUND GSTREAMER_FOUND QT_FOUND)
# to_yes_no(USE_GTK2 USE_RSVG2 USE_SNDFILE USE_MPG123 USE_MPCDEC USE_SPEEX_RESAMPLER USE_AVFORMAT USE_TAGLIB USE_GSTREAMER USE_QT)
# 
# ##### Print status
# message(STATUS "status          found / use --")
# message(STATUS "gtk-2.0:        ${GTK2_FOUND}"     "     ${USE_GTK2}")
# message(STATUS "rsvg2:          ${RSVG2_FOUND}"    "     ${USE_RSVG2}")
# message(STATUS "libsndfile:     ${SNDFILE_FOUND}"  "     ${USE_SNDFILE}")
# message(STATUS "libmpg123:      ${MPG123_FOUND}"   "     ${USE_MPG123}")
# message(STATUS "libmpcdec:      ${MPCDEC_FOUND}"   "     ${USE_MPCDEC}")
# message(STATUS "speexdsp:       ${SPEEXDSP_FOUND}" "     ${USE_SPEEX_RESAMPLER}")
# message(STATUS "ffmpeg:         ${AVFORMAT_FOUND}" "     ${USE_AVFORMAT}")
# message(STATUS "taglib:         ${TAGLIB_FOUND}"   "     ${USE_TAGLIB}")
# message(STATUS "gstreamer:      ${GSTREAMER_FOUND}""     ${USE_GSTREAMER}")
# message(STATUS "qt4:            ${QT_FOUND}"       "     ${USE_QT}")

# ######################################
# ##### Install                    #####
# ######################################
# 
# #ebur128 install stuff
# if(FOLDER_INCLUDE)
#     install(FILES ebur128.h DESTINATION ${FOLDER_INCLUDE})
# endif()
# 
# # install(TARGETS ebur128 ebur128_static
# #         LIBRARY DESTINATION ${FOLDER_LIB}
# #         ARCHIVE DESTINATION ${FOLDER_LIB})
# 
# # set up install folders
# set(FLAT_INSTALL FALSE CACHE BOOL "Install everything in one folder")
# if(FLAT_INSTALL)
#   set(FOLDER_DOC .)
#   set(FOLDER_BIN .)
#   set(FOLDER_LIB .)
#   set(FOLDER_PLUGIN .)
# else()
#   set(FOLDER_BIN ${CMAKE_INSTALL_BINDIR})
#   set(FOLDER_LIB ${CMAKE_INSTALL_LIBDIR})
#   set(FOLDER_PLUGIN ${CMAKE_INSTALL_LIBDIR}/ebur128-loudness-tools)
#   set(FOLDER_INCLUDE ${CMAKE_INSTALL_INCLUDEDIR})
# endif()
# set(PLUGIN_PATH "${CMAKE_INSTALL_PREFIX}/${FOLDER_PLUGIN}")
# 
# if(FOLDER_DOC)
#     install(FILES COPYING README.md DESTINATION ${FOLDER_DOC})
#     install(DIRECTORY doc/license DESTINATION ${FOLDER_DOC})
# endif()
# 
# if(WIN32)
#   if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
#     set(DLL_FOLDER dlls-64)
#   else()
#     set(DLL_FOLDER dlls-32)
#   endif()
#   install(DIRECTORY ${DLL_FOLDER}/ DESTINATION ${FOLDER_BIN}
#           FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
#                            GROUP_EXECUTE GROUP_READ
#                            WORLD_EXECUTE WORLD_READ)
# endif()
# 
# if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND AVFORMAT_FOUND AND
#    TAGLIB_FOUND)
#   set(CPACK_PACKAGE_VERSION_MAJOR "1")
#   set(CPACK_PACKAGE_VERSION_MINOR "0")
#   set(CPACK_PACKAGE_VERSION_PATCH "0")
# 
#   set(CPACK_GENERATOR ZIP TGZ)
#   set(CPACK_SOURCE_GENERATOR ZIP TGZ)
#   set(CPACK_STRIP_FILES TRUE)
# 
#   set(CPACK_SOURCE_IGNORE_FILES "/TODO;/build.*/;/.git/;/.gitignore;/.gitmodules;/mingw.*/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")
# 
#   set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
# 
#   include(CPack)
# endif()
