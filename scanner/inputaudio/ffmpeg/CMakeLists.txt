cmake_minimum_required(VERSION 2.8)

include(utils)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAVFORMAT libavformat)
pkg_check_modules(GMODULE20 gmodule-2.0)

if(LIBAVFORMAT_FOUND AND GMODULE20_FOUND AND
   INPUT_INCLUDE_DIR AND EBUR128_INCLUDE_DIR)
  add_library(input_ffmpeg MODULE input_ffmpeg.c mp3_padding.c mp4_padding.c ../input_helper.c)

  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})



  list(APPEND INPUT_FFMPEG_CFLAGS  ${LIBAVFORMAT_CFLAGS}  ${GMODULE20_CFLAGS})
  list(APPEND INPUT_FFMPEG_LDFLAGS ${LIBAVFORMAT_LDFLAGS} ${GMODULE20_LDFLAGS})

  to_space_list(INPUT_FFMPEG_CFLAGS)
  to_space_list(INPUT_FFMPEG_LDFLAGS)

  set_target_properties(input_ffmpeg PROPERTIES
          COMPILE_FLAGS ${INPUT_FFMPEG_CFLAGS}
             LINK_FLAGS ${INPUT_FFMPEG_LDFLAGS})

  # if(DO_INSTALL)
  #   install(TARGETS input_ffmpeg LIBRARY DESTINATION ${FOLDER_PLUGIN})
  # endif()
else()
  message(STATUS "-----------------------------------------------------------")
  message(WARNING "Could not find dependencies for input_ffmpeg plugin:")
  to_yes_no(LIBAVFORMAT_FOUND GMODULE20_FOUND)
  if_empty_print_missing(INPUT_INCLUDE_DIR EBUR128_INCLUDE_DIR)
  message(STATUS "input_ffmpeg dependency libavformat: ${LIBAVFORMAT_FOUND}")
  message(STATUS "input_ffmpeg dependency gmodule-2.0: ${GMODULE20_FOUND}")
  message(STATUS "input_ffmpeg dependency input.h:     ${INPUT_INCLUDE_DIR}")
  message(STATUS "input_ffmpeg dependency ebur128.h:   ${EBUR128_INCLUDE_DIR}")
  message(STATUS "-----------------------------------------------------------")
endif()
