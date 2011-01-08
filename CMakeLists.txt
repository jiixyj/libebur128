######################################
##### CMake environment setup    #####
######################################
project(ebu-r128)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${${PROJECT_NAME}_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})
if(IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include)
  include_directories(SYSTEM ${${PROJECT_NAME}_SOURCE_DIR}/include)
endif()


######################################
##### Library setup              #####
######################################

##### libsndfile
include(FindPkgConfig)
pkg_check_modules(SNDFILE REQUIRED sndfile>=1.0.23)
include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})


######################################
##### Compiler setup             #####
######################################
include("${CMAKE_MODULE_PATH}/CompilerFlags.txt")  # This sets VALID_FLAGS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${VALID_FLAGS}")


add_library(ebur128 src/ebur128.c)
add_executable(r128-test src/main.c)
target_link_libraries(r128-test ebur128 ${SNDFILE_LIBRARIES})
