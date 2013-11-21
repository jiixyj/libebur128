cmake_minimum_required(VERSION 2.8)

project(ebur128-loudness-tools)
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
