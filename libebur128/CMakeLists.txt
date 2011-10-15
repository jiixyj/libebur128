cmake_minimum_required(VERSION 2.6.4)

include_directories(queue)
include_directories(speex_resample)

# set compile definitions/flags for the speex resampler
set(SPEEX_COMPILE_DEFINITIONS
  OUTSIDE_SPEEX
  RANDOM_PREFIX=ebur128
  FLOATING_POINT
  EXPORT=
)

if(USE_SSE2 OR ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
  list(APPEND SPEEX_COMPILE_DEFINITIONS _USE_SSE _USE_SSE2)
endif()
set_property(SOURCE speex_resample/resample.c
             PROPERTY COMPILE_DEFINITIONS
             ${SPEEX_COMPILE_DEFINITIONS})

add_library(ebur128 ebur128.c speex_resample/resample.c)
if(EBUR128_LINK_AGAINST_SCANNER)
  set_target_properties(ebur128 PROPERTIES COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
endif()

if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()
