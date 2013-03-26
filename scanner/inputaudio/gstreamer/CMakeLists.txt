cmake_minimum_required(VERSION 2.8)
include(utils)

find_package(PkgConfig)
pkg_check_modules(GMODULE20 gmodule-2.0)
pkg_check_modules(GSTREAMER_APP   gstreamer-app-0.10)
pkg_check_modules(GSTREAMER_AUDIO gstreamer-audio-0.10)

if(GMODULE20_FOUND AND GSTREAMER_APP_FOUND AND GSTREAMER_AUDIO_FOUND AND
   INPUT_INCLUDE_DIR AND EBUR128_INCLUDE_DIR)
  add_library(input_gstreamer MODULE input_gstreamer.c)


  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})


  list(APPEND INPUT_GSTREAMER_CFLAGS  ${GMODULE20_CFLAGS}  ${GSTREAMER_APP_CFLAGS}  ${GSTREAMER_AUDIO_CFLAGS})
  list(APPEND INPUT_GSTREAMER_LDFLAGS ${GMODULE20_LDFLAGS} ${GSTREAMER_APP_LDFLAGS} ${GSTREAMER_AUDIO_LDFLAGS})

  to_space_list(INPUT_GSTREAMER_CFLAGS)
  to_space_list(INPUT_GSTREAMER_LDFLAGS)

  set_target_properties(input_gstreamer PROPERTIES
          COMPILE_FLAGS ${INPUT_GSTREAMER_CFLAGS}
             LINK_FLAGS ${INPUT_GSTREAMER_LDFLAGS})




else()
  message(STATUS "-----------------------------------------------------------")
  message(WARNING "Could not find dependencies for input_gstreamer plugin:")
  to_yes_no(GSTREAMER_APP_FOUND GSTREAMER_AUDIO_FOUND GMODULE20_FOUND)
  if_empty_print_missing(INPUT_INCLUDE_DIR EBUR128_INCLUDE_DIR)
  message(STATUS "input_gstreamer dependency gstreamer-app:   ${GSTREAMER_APP_FOUND}")
  message(STATUS "input_gstreamer dependency gstreamer-audio: ${GSTREAMER_AUDIO_FOUND}")
  message(STATUS "input_gstreamer dependency gmodule-2.0:     ${GMODULE20_FOUND}")
  message(STATUS "input_gstreamer dependency input.h:         ${INPUT_INCLUDE_DIR}")
  message(STATUS "input_gstreamer dependency ebur128.h:       ${EBUR128_INCLUDE_DIR}")
  message(STATUS "-----------------------------------------------------------")
endif()
