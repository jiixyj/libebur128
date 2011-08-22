cmake_minimum_required(VERSION 2.6.4)

include_directories(queue)
include_directories(speex_resample)

set(CMAKE_C_FLAGS_ORIGINAL "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS)


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
# Use C99 if available
if(FLAG_STD_C99)
  set_property(SOURCE speex_resample/resample.c PROPERTY COMPILE_FLAGS "-std=c99 ${CMAKE_C_FLAGS_R128}")
else()
  set_property(SOURCE speex_resample/resample.c PROPERTY COMPILE_FLAGS ${CMAKE_C_FLAGS_R128})
endif()


# use normal CMAKE_C_FLAGS for ebur128.c
set_property(SOURCE ebur128.c PROPERTY COMPILE_FLAGS ${CMAKE_C_FLAGS_ORIGINAL})


add_library(ebur128 ebur128.c speex_resample/resample.c)
if(EBUR128_LINK_AGAINST_SCANNER)
  set_target_properties(ebur128 PROPERTIES COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS}")
endif()

if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()
