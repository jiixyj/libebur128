cmake_minimum_required(VERSION 2.8)

include(utils)

find_package(MPCDEC)
find_package(PkgConfig)
pkg_check_modules(GMODULE20 gmodule-2.0)

if(GMODULE20_FOUND AND MPCDEC_FOUND AND
   INPUT_INCLUDE_DIR AND EBUR128_INCLUDE_DIR)
  add_library(input_musepack MODULE input_mpcdec.c)

  include_directories(SYSTEM ${MPCDEC_INCLUDE_DIRS})
  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})
  target_link_libraries(input_musepack ${MPCDEC_LIBRARIES})

  list(APPEND INPUT_MPCDEC_CFLAGS  ${GMODULE20_CFLAGS})
  list(APPEND INPUT_MPCDEC_LDFLAGS ${GMODULE20_LDFLAGS})

  to_space_list(INPUT_MPCDEC_CFLAGS)
  to_space_list(INPUT_MPCDEC_LDFLAGS)

  set_target_properties(input_musepack PROPERTIES
          COMPILE_FLAGS ${INPUT_MPCDEC_CFLAGS}
             LINK_FLAGS ${INPUT_MPCDEC_LDFLAGS})
  # if(WIN32)
  #     target_link_libraries(input_musepack ${GLIB_LIBRARY})
  # endif()
  # install(TARGETS input_musepack LIBRARY DESTINATION ${FOLDER_PLUGIN})
else()
  message(STATUS "-----------------------------------------------------------")
  message(WARNING "Could not find dependencies for input_mpcdec plugin:")
  to_yes_no(MPCDEC_FOUND GMODULE20_FOUND)
  if_empty_print_missing(INPUT_INCLUDE_DIR EBUR128_INCLUDE_DIR)
  message(STATUS "input_mpcdec dependency mpcdec:      ${MPCDEC_FOUND}")
  message(STATUS "input_mpcdec dependency gmodule-2.0: ${GMODULE20_FOUND}")
  message(STATUS "input_mpcdec dependency input.h:     ${INPUT_INCLUDE_DIR}")
  message(STATUS "input_mpcdec dependency ebur128.h:   ${EBUR128_INCLUDE_DIR}")
  message(STATUS "-----------------------------------------------------------")
endif()
