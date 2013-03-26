cmake_minimum_required(VERSION 2.8)

include(utils)

find_package(PkgConfig)
pkg_check_modules(LIBAVFORMAT libavformat)
pkg_check_modules(GMODULE20 gmodule-2.0)

set(SUMMARY_FFMPEG_FOUND ${LIBAVFORMAT_FOUND} CACHE INTERNAL "")

if(LIBAVFORMAT_FOUND AND GMODULE20_FOUND AND
   INPUT_INCLUDE_DIR AND EBUR128_INCLUDE_DIR AND
   NOT DISABLE_GLIB20 AND NOT DISABLE_FFMPEG)
  add_library(input_ffmpeg MODULE input_ffmpeg.c mp3_padding.c mp4_padding.c ../input_helper.c)

  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})



  list(APPEND INPUT_FFMPEG_CFLAGS  ${LIBAVFORMAT_CFLAGS}  ${GMODULE20_CFLAGS})
  list(APPEND INPUT_FFMPEG_LDFLAGS ${LIBAVFORMAT_LDFLAGS} ${GMODULE20_LDFLAGS})

  to_space_list(INPUT_FFMPEG_CFLAGS)
  to_space_list(INPUT_FFMPEG_LDFLAGS)

  set_target_properties(input_ffmpeg PROPERTIES
          COMPILE_FLAGS ${INPUT_FFMPEG_CFLAGS}
             LINK_FLAGS ${INPUT_FFMPEG_LDFLAGS})
endif()
