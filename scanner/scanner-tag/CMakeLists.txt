add_library(scanner-tag scanner-tag rgtag)

include_directories(${TAGLIB_INCLUDE_DIRS})

target_link_libraries(scanner-tag scanner-common ${TAGLIB_LIBRARIES})
