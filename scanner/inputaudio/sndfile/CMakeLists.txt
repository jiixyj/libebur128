cmake_minimum_required(VERSION 2.8)

include(utils)

find_package(SNDFILE)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GMODULE20 gmodule-2.0)

if(GMODULE20_FOUND AND SNDFILE_FOUND AND
   INPUT_INCLUDE_DIR AND EBUR128_INCLUDE_DIR)
  add_library(input_sndfile MODULE input_sndfile.c ../input_helper.c)

  include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})
  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})
  target_link_libraries(input_sndfile ${SNDFILE_LIBRARIES})

  list(APPEND INPUT_SNDFILE_CFLAGS  ${GMODULE20_CFLAGS})
  list(APPEND INPUT_SNDFILE_LDFLAGS ${GMODULE20_LDFLAGS})

  to_space_list(INPUT_SNDFILE_CFLAGS)
  to_space_list(INPUT_SNDFILE_LDFLAGS)

  set_target_properties(input_sndfile PROPERTIES
          COMPILE_FLAGS ${INPUT_SNDFILE_CFLAGS}
             LINK_FLAGS ${INPUT_SNDFILE_LDFLAGS})




else()
  message(STATUS "-----------------------------------------------------------")
  message(WARNING "Could not find dependencies for input_sndfile plugin:")
  to_yes_no(SNDFILE_FOUND GMODULE20_FOUND)
  if_empty_print_missing(INPUT_INCLUDE_DIR EBUR128_INCLUDE_DIR)
  message(STATUS "input_sndfile dependency sndfile:     ${SNDFILE_FOUND}")
  message(STATUS "input_sndfile dependency gmodule-2.0: ${GMODULE20_FOUND}")
  message(STATUS "input_sndfile dependency input.h:     ${INPUT_INCLUDE_DIR}")
  message(STATUS "input_sndfile dependency ebur128.h:   ${EBUR128_INCLUDE_DIR}")
  message(STATUS "-----------------------------------------------------------")
endif()
