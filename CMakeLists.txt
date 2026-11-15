cmake_minimum_required(VERSION 3.23)
project(libebur128 C)

include(CTest)
if(BUILD_TESTING)
  enable_language(CXX)
endif()

option(BUILD_SHARED_LIBS
    "Build shared libraries instead of static ones" ON)
option(EBUR128_BUILD_EXAMPLES "Build example programs" OFF)
option(EBUR128_COVERAGE "Add LLVM source-based coverage instrumentation" OFF)
if(EBUR128_COVERAGE)
  add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
  add_link_options(-fprofile-instr-generate)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

include(utils)
include(GNUInstallDirs)

add_subdirectory(ebur128)
if(EBUR128_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

to_yes_no(SUMMARY_HAS_QUEUE)

if(ENABLE_INTERNAL_QUEUE_H)
  set(USE_QUEUE "using own copy of queue.h")
else()
  set(USE_QUEUE "using system copy of queue.h")
endif()

##### Print status
message(STATUS "Status          found / disabled --")
message(STATUS "queue.h:        ${SUMMARY_HAS_QUEUE}"        "     ${USE_QUEUE}")

if(BUILD_SHARED_LIBS)
  message(STATUS "Building shared library (set BUILD_SHARED_LIBS to OFF to build static)")
else()
  message(STATUS "Building static library")
endif()

if(EBUR128_BUILD_EXAMPLES)
  message(STATUS "building examples!")
else()
  message(STATUS "not building examples, set EBUR128_BUILD_EXAMPLES to ON to enable")
endif()
