cmake_minimum_required(VERSION 3.10)
project(libebur128 C)

include(GNUInstallDirs)

# Include the add_feature_option macro
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(AddFeatureOption) # includes FetureSummary

add_feature_option(BUILD_SHARED_LIBS "Build shared libraries" ON)
add_feature_option(ENABLE_TESTS "Build test binaries (requires libsndfile)" OFF)
add_feature_option(ENABLE_FUZZER "Build fuzzer binary (requires clang++)" OFF)
add_feature_option(ENABLE_INTERNAL_QUEUE_H "Use bundled queue.h" OFF)

if(ENABLE_FUZZER)
  enable_language(CXX)
  set(SANITIZER_FLAGS "-fsanitize=address,undefined,unsigned-integer-overflow")
  set(FUZZER_FLAGS "${SANITIZER_FLAGS},fuzzer")
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(ebur128)
add_subdirectory(test)

feature_summary(WHAT ALL)
