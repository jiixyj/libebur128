######################################
##### CMake environment setup    #####
######################################
project(libebur128 C)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${${PROJECT_NAME}_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})
link_directories(${${PROJECT_NAME}_SOURCE_DIR}/lib)
if(IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include)
  include_directories(SYSTEM ${${PROJECT_NAME}_SOURCE_DIR}/include)
endif()


######################################
##### Library setup              #####
######################################

##### libsndfile
if(NOT WIN32)
  include(FindPkgConfig)
  pkg_check_modules(SNDFILE REQUIRED sndfile>=1.0.21)
  include_directories(SYSTEM ${SNDFILE_INCLUDEDIR})
else()
  set(SNDFILE_LIBRARIES sndfile-1)
endif()

##### ffmpeg
if(NOT WIN32)
  pkg_check_modules(LIBAVCODEC libavcodec)
  pkg_check_modules(LIBAVFORMAT libavformat)
  include_directories(SYSTEM ${LIBAVCODEC_INCLUDEDIR})
  include_directories(SYSTEM ${LIBAVFORMAT_INCLUDEDIR})
else()
  set(LIBAVCODEC_LIBRARIES avcodec-52)
  set(LIBAVFORMAT_LIBRARIES avformat-52)
  set(LIBAVUTIL_LIBRARIES avutil-50)
endif()


######################################
##### Compiler setup             #####
######################################
include("${CMAKE_MODULE_PATH}/CompilerFlags.txt")  # This sets VALID_FLAGS
set(CMAKE_C_FLAGS_OLD "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_OLD} -ansi ${VALID_FLAGS}")


######################################
##### Add libraries/executables  #####
######################################
add_library(ebur128 src/ebur128.c)

add_executable(r128-sndfile src/sndfile_example.c)
target_link_libraries(r128-sndfile ebur128 ${SNDFILE_LIBRARIES})

add_executable(r128-minimal-example src/minimal_example.c)
target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_OLD} -std=c99 ${VALID_FLAGS}")
add_executable(r128-ffmpeg src/ffmpeg_example.c)
target_link_libraries(r128-ffmpeg ebur128 ${LIBAVCODEC_LIBRARIES} ${LIBAVFORMAT_LIBRARIES} ${LIBAVUTIL_LIBRARIES})


######################################
##### Install                    #####
######################################
install(TARGETS r128-sndfile r128-ffmpeg
        RUNTIME DESTINATION .
        ARCHIVE DESTINATION lib)
install(FILES LICENSE DESTINATION .)
install(DIRECTORY doc/license DESTINATION .)
if(WIN32)
  install(FILES lib/libsndfile-1.dll DESTINATION .)
endif()

set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "2")

set(CPACK_BINARY_ZIP ON)
set(CPACK_BINARY_NSIS OFF)
set(CPACK_SOURCE_ZIP ON)
set(CPACK_SOURCE_IGNORE_FILES "build/;.git/;.gitignore;lib/;mingw/;lsb/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

include(CPack)
