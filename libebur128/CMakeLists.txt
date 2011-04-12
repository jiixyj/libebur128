include_directories(queue)
include_directories(speex_resample)

add_library(ebur128 ebur128.c)
if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()

add_subdirectory(speex_resample)
target_link_libraries(ebur128 speex_resample)
