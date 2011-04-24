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
set_property(SOURCE speex_resample/resample.c PROPERTY COMPILE_FLAGS ${CMAKE_C_FLAGS_R128})
# Use C99 if available
if(FLAG_STD_C99)
  set_property(SOURCE speex_resample/resample.c APPEND PROPERTY COMPILE_FLAGS "-std=c99")
endif()


# use normal CMAKE_C_FLAGS for ebur128.c
set_property(SOURCE ebur128.c PROPERTY COMPILE_FLAGS ${CMAKE_C_FLAGS_ORIGINAL})


add_library(ebur128 ebur128.c speex_resample/resample.c)

if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()
