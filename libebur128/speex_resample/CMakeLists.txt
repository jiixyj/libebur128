set(SPEEX_COMPILE_DEFINITIONS
  OUTSIDE_SPEEX
  RANDOM_PREFIX=ebur128
  FLOATING_POINT
  EXPORT=
)
if(USE_SSE2 OR ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
  list(APPEND SPEEX_COMPILE_DEFINITIONS _USE_SSE _USE_SSE2)
endif()

set_property(DIRECTORY
             PROPERTY COMPILE_DEFINITIONS
             ${SPEEX_COMPILE_DEFINITIONS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_R128}")
add_library(speex_resample resample.c)

# Use C99 if available
if(FLAG_STD_C99)
  set_target_properties(speex_resample PROPERTIES COMPILE_FLAGS "-std=c99")
endif()
