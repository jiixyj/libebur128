include_directories(${SCANNERCOMMON_INCLUDE_DIRS})

add_library(scanner-common parse_args nproc scanner-common)

set_target_properties(scanner-common PROPERTIES
        COMPILE_FLAGS ${GLIB20_CFLAGS}
           LINK_FLAGS ${GLIB20_LDFLAGS})

if(USE_SNDFILE)
    include_directories(${SNDFILE_INCLUDE_DIRS})
    target_link_libraries(scanner-common ${SNDFILE_LIBRARIES})
endif()
