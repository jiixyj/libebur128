cmake_minimum_required(VERSION 2.6.4)

find_path(QUEUE_INCLUDE_PATH sys/queue.h)
if(QUEUE_INCLUDE_PATH)
    include_directories(SYSTEM ${QUEUE_INCLUDE_PATH})
else()
    include_directories(SYSTEM queue)
endif()

set(EBUR128_VERSION_MAJOR 1)
set(EBUR128_VERSION 1.0.0)

add_library(ebur128_static STATIC ebur128.c)
set_target_properties(ebur128_static PROPERTIES
    OUTPUT_NAME ebur128)
add_library(ebur128 SHARED ebur128.c)
set_target_properties(ebur128 PROPERTIES
    SOVERSION ${EBUR128_VERSION_MAJOR}
    VERSION ${EBUR128_VERSION}
    LINK_INTERFACE_LIBRARIES "")

if(USE_SPEEX_RESAMPLER)
    include_directories(SYSTEM ${SPEEXDSP_INCLUDE_DIRS})
    target_link_libraries(ebur128 ${SPEEXDSP_LIBRARIES})
endif()

if(M_LIBRARY)
    target_link_libraries(ebur128 ${M_LIBRARY})
endif()

if(FOLDER_INCLUDE)
    install(FILES ebur128.h DESTINATION ${FOLDER_INCLUDE})
endif()

install(TARGETS ebur128 ebur128_static
        LIBRARY DESTINATION ${FOLDER_LIB}
        ARCHIVE DESTINATION ${FOLDER_LIB})
