cmake_minimum_required(VERSION 2.8.9)

set(WITH_STATIC_PIC         OFF CACHE BOOL "Compile static library with -fPIC flag")
set(ENABLE_INTERNAL_QUEUE_H OFF CACHE BOOL "Use own queue.h")
set(DISABLE_SPEEXDSP        OFF CACHE BOOL "Don't build with speexdsp")


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
    find_pkg_config(SPEEXDSP speexdsp)
    if(SPEEXDSP_FOUND)
        include_directories(SYSTEM ${SPEEXDSP_INCLUDE_DIRS})
        add_definitions(-DUSE_SPEEX_RESAMPLER)
    endif()
endif()


if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
endif()


set(EBUR128_VERSION_MAJOR 1)
set(EBUR128_VERSION 1.0.1)

add_library(ebur128_static STATIC ebur128.c)
set_property(TARGET ebur128_static PROPERTY OUTPUT_NAME ebur128)
if(WITH_STATIC_PIC)
    set_property(TARGET ebur128_static PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

add_library(ebur128 SHARED ebur128.c)
set_target_properties(ebur128 PROPERTIES
    SOVERSION ${EBUR128_VERSION_MAJOR}
    VERSION ${EBUR128_VERSION})

if(UNIX)
  target_link_libraries(ebur128 -lm)
endif(UNIX)

if(SPEEXDSP_FOUND AND NOT DISABLE_SPEEXDSP)
    set_property(TARGET ebur128_static APPEND_STRING PROPERTY
        COMPILE_FLAGS " ${SPEEXDSP_CFLAGS}")
    set_property(TARGET ebur128 APPEND_STRING PROPERTY
        COMPILE_FLAGS " ${SPEEXDSP_CFLAGS}")
    target_link_libraries(ebur128 ${SPEEXDSP_LIBRARIES})
endif()


set(SUMMARY_SPEEXDSP_FOUND ${SPEEXDSP_FOUND} CACHE INTERNAL "")
set(EBUR128_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")

install(FILES ebur128.h DESTINATION include)
install(TARGETS ebur128 ebur128_static DESTINATION lib)
