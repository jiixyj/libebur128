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
