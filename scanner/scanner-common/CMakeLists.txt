include_directories(${SCANNERCOMMON_INCLUDE_DIRS})

add_library(scanner-common parse_args nproc scanner-common)
target_link_libraries(scanner-common ebur128)

if(SNDFILE_FOUND AND NOT DISABLE_SNDFILE)
  include_directories(${SNDFILE_INCLUDE_DIRS})
  add_definitions(-DUSE_SNDFILE)
  target_link_libraries(scanner-common ${SNDFILE_LIBRARIES})
endif()

if(SPEEXDSP_FOUND AND NOT DISABLE_SPEEXDSP)
  add_definitions(-DUSE_SPEEX_RESAMPLER)
endif()

set_target_properties(scanner-common PROPERTIES
        COMPILE_FLAGS "${GLIB20_CFLAGS} ${SPEEXDSP_CFLAGS}")
