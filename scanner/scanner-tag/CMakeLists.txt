add_library(scanner-common-tag scanner-tag rgtag)

include_directories(${TAGLIB_INCLUDE_DIRS})

target_link_libraries(scanner-common-tag scanner-common ${TAGLIB_LIBRARIES})
