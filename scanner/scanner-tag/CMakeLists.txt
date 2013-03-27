find_package(TAGLIB)

set(SUMMARY_TAGLIB_FOUND ${TAGLIB_FOUND} CACHE INTERNAL "")

if(TAGLIB_FOUND AND NOT DISABLE_TAGLIB)
  include_directories(SYSTEM ${GLIB20_INCLUDE_DIRS})
  add_definitions(           ${GLIB20_CFLAGS_OTHER})

  add_library(scanner-tag scanner-tag rgtag)
  include_directories(${TAGLIB_INCLUDE_DIRS})
  target_link_libraries(scanner-tag scanner-common ${TAGLIB_LIBRARIES})
endif()
