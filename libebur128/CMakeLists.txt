cmake_minimum_required(VERSION 2.6.4)

include_directories(queue)

add_library(ebur128 ebur128.c)
target_link_libraries(ebur128 speexdsp)

if(EBUR128_LINK_AGAINST_SCANNER)
  set_target_properties(ebur128 PROPERTIES COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
endif()

if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()
