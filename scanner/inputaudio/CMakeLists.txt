cmake_minimum_required(VERSION 2.8)

include(utils)

find_package(PkgConfig)
pkg_check_modules(GMODULE20 gmodule-2.0)

if(GMODULE20_FOUND AND NOT DISABLE_GLIB20)
  set(INPUT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})


  add_library(input input)

  list(APPEND INPUT_CFLAGS  ${GMODULE20_CFLAGS})
  list(APPEND INPUT_LDFLAGS ${GMODULE20_LDFLAGS})

  to_space_list(INPUT_CFLAGS)
  to_space_list(INPUT_LDFLAGS)

  set(INPUT_LDFLAGS ${INPUT_LDFLAGS} PARENT_SCOPE)

  set_target_properties(input PROPERTIES
          COMPILE_FLAGS ${INPUT_CFLAGS})


  add_subdirectory(sndfile)
  add_subdirectory(mpg123)
  add_subdirectory(mpcdec)
  add_subdirectory(ffmpeg)
  add_subdirectory(gstreamer)
endif()
