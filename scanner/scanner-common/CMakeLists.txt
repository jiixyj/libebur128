add_library(scanner-common parse_args nproc scanner-common)

if(USE_SNDFILE)
    include_directories(${SNDFILE_INCLUDE_DIRS})
    target_link_libraries(scanner-common ${SNDFILE_LIBRARIES})
endif()

target_link_libraries(scanner-common ebur128 input filetree ${GLIB_LIBRARY} ${GTHREAD_LIBRARY})
