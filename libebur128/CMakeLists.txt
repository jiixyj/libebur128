cmake_minimum_required(VERSION 2.6.4)

include_directories(queue)

add_library(ebur128 ebur128.c)

if(SPEEXDSP_FOUND)
  include_directories(SYSTEM ${SPEEXDSP_INCLUDE_DIRS})
  target_link_libraries(ebur128 ${SPEEXDSP_LIBRARIES})
endif()

if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()
