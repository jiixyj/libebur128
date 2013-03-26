cmake_minimum_required(VERSION 2.8)

include(utils)

find_package(TAGLIB)
find_package(PkgConfig)
pkg_check_modules(GLIB20 glib-2.0)

set(SUMMARY_TAGLIB_FOUND ${TAGLIB_FOUND} CACHE INTERNAL "")

if(TAGLIB_FOUND AND NOT DISABLE_TAGLIB AND
   GLIB20_FOUND AND NOT DISABLE_GLIB20)
  add_library(scanner-tag scanner-tag rgtag)
  include_directories(
    ${TAGLIB_INCLUDE_DIRS}
    ${SCANNERCOMMON_INCLUDE_DIRS})
  target_link_libraries(scanner-tag scanner-common ${TAGLIB_LIBRARIES})

  to_space_list(GLIB20_CFLAGS)
  set_target_properties(scanner-tag PROPERTIES
          COMPILE_FLAGS ${GLIB20_CFLAGS})
endif()
