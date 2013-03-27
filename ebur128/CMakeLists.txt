cmake_minimum_required(VERSION 2.8)
project(ebur128 C)

#### queue.h
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/queuetest.c
"#include <sys/queue.h>\nLIST_HEAD(listhead, entry) head;\nint main() { return 0; }")
try_compile(HAS_QUEUE ${CMAKE_CURRENT_BINARY_DIR}
                      ${CMAKE_CURRENT_BINARY_DIR}/queuetest.c)

set(SUMMARY_HAS_QUEUE ${HAS_QUEUE} CACHE INTERNAL "")

if(ENABLE_INTERNAL_QUEUE_H)
    include_directories(SYSTEM queue)
endif()


#### speexdsp
if(NOT DISABLE_SPEEXDSP)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SPEEXDSP speexdsp)
    endif()

    if(SPEEXDSP_FOUND)
        include_directories(SYSTEM ${SPEEXDSP_INCLUDE_DIRS})
        link_directories(${SPEEXDSP_LIBRARY_DIRS})
        add_definitions(${SPEEXDSP_CFLAGS_OTHER})
        add_definitions(-DUSE_SPEEX_RESAMPLER)
    endif()
endif()


if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
endif()

set(EBUR128_VERSION_MAJOR 1)
set(EBUR128_VERSION 1.0.0)

add_library(ebur128_static STATIC ebur128.c)
set_target_properties(ebur128_static PROPERTIES
    OUTPUT_NAME ebur128)
add_library(ebur128 SHARED ebur128.c)
set_target_properties(ebur128 PROPERTIES
    SOVERSION ${EBUR128_VERSION_MAJOR}
    VERSION ${EBUR128_VERSION})

target_link_libraries(ebur128 ${SPEEXDSP_LIBRARIES})

set(SUMMARY_SPEEXDSP_FOUND ${SPEEXDSP_FOUND} CACHE INTERNAL "")
set(EBUR128_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")
