cmake_minimum_required(VERSION 2.8)

include(utils)

find_package(MPG123)
find_package(PkgConfig)
pkg_check_modules(GMODULE20 gmodule-2.0)

set(SUMMARY_MPG123_FOUND ${MPG123_FOUND} CACHE INTERNAL "")

if(GMODULE20_FOUND AND MPG123_FOUND AND
   INPUT_INCLUDE_DIR AND EBUR128_INCLUDE_DIR AND
   NOT DISABLE_GLIB20 AND NOT DISABLE_MPG123)
  add_library(input_mpg123 MODULE input_mpg123.c)

  include_directories(SYSTEM ${MPG123_INCLUDE_DIRS})
  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})
  target_link_libraries(input_mpg123 ${MPG123_LIBRARIES})

  list(APPEND INPUT_MPG123_CFLAGS  ${GMODULE20_CFLAGS})
  list(APPEND INPUT_MPG123_LDFLAGS ${GMODULE20_LDFLAGS})

  to_space_list(INPUT_MPG123_CFLAGS)
  to_space_list(INPUT_MPG123_LDFLAGS)

  set_target_properties(input_mpg123 PROPERTIES
          COMPILE_FLAGS ${INPUT_MPG123_CFLAGS}
             LINK_FLAGS ${INPUT_MPG123_LDFLAGS})
endif()
