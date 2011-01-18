######################################
##### CMake environment setup    #####
######################################
project(libebur128 C)
cmake_minimum_required(VERSION 2.6.4)
set(CMAKE_MODULE_PATH ${${PROJECT_NAME}_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${${PROJECT_NAME}_BINARY_DIR})
link_directories(${${PROJECT_NAME}_SOURCE_DIR}/lib)
link_directories(${${PROJECT_NAME}_SOURCE_DIR}/lib64)
if(IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include)
  include_directories(SYSTEM ${${PROJECT_NAME}_SOURCE_DIR}/include)
endif()
if(WIN32 AND IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/mingw-include)
  include_directories(SYSTEM ${${PROJECT_NAME}_SOURCE_DIR}/mingw-include)
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

##### mpg123
if(NOT WIN32)
  pkg_check_modules(MPG123 libmpg123)
  include_directories(SYSTEM ${MPG123_INCLUDEDIR})
else()
  set(MPG123_LIBRARIES mpg123-0)
endif()

##### mpddec
if(WIN32)
  list(REVERSE CMAKE_FIND_LIBRARY_PREFIXES)
endif()
if(IS_DIRECTORY ${${PROJECT_NAME}_SOURCE_DIR}/include/mpc)
  find_library(MPCDEC_LIBRARY mpcdec_static
               PATHS ${${PROJECT_NAME}_SOURCE_DIR}/lib
                     ${${PROJECT_NAME}_SOURCE_DIR}/lib64
               CMAKE_FIND_ROOT_PATH_BOTH)
  if(MPCDEC_LIBRARY)
    set(MPCDEC_FOUND TRUE)
  else()
    set(MPCDEC_FOUND FALSE)
    message(STATUS "Could not find the mpcdec library! Will not build mpcdec example.")
  endif()
else()
  set(MPCDEC_FOUND FALSE)
  message(STATUS "Could not find mpc include dir! Will not build mpcdec example.")
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
if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()

add_executable(r128-minimal-example src/minimal_example.c)
target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})

add_executable(r128-sndfile src/sndfile_example.c)
target_link_libraries(r128-sndfile ebur128 ${SNDFILE_LIBRARIES})

add_executable(r128-mpg123 src/mpg123_example.c)
target_link_libraries(r128-mpg123 ebur128 ${MPG123_LIBRARIES})

if(MPCDEC_FOUND)
  add_executable(r128-musepack src/mpcdec_example.c)
  target_link_libraries(r128-musepack ${MPCDEC_LIBRARY} ebur128)
  set_target_properties(r128-musepack PROPERTIES COMPILE_FLAGS "-std=c99")
endif()

add_executable(r128-ffmpeg src/ffmpeg_example.c)
target_link_libraries(r128-ffmpeg ebur128 ${LIBAVCODEC_LIBRARIES} ${LIBAVFORMAT_LIBRARIES} ${LIBAVUTIL_LIBRARIES})
set_target_properties(r128-ffmpeg PROPERTIES COMPILE_FLAGS "-std=c99")

add_executable(r128-test-library src/tests.c)
target_link_libraries(r128-test-library ebur128 ${SNDFILE_LIBRARIES})
set_target_properties(r128-test-library PROPERTIES COMPILE_FLAGS "-Wno-float-equal")


######################################
##### Install                    #####
######################################
install(TARGETS r128-sndfile r128-ffmpeg r128-mpg123 r128-musepack
        RUNTIME DESTINATION .
        ARCHIVE DESTINATION lib)
install(FILES LICENSE DESTINATION .)
install(PROGRAMS src/rgtag.py DESTINATION .)
install(DIRECTORY doc/license DESTINATION .)
if(CMAKE_C_COMPILER MATCHES ".*w64.*")
  install(FILES lib64/libsndfile-1.dll DESTINATION .)
elseif(WIN32)
  install(FILES lib/libsndfile-1.dll DESTINATION .)
endif()

set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "4")

set(CPACK_BINARY_ZIP ON)
set(CPACK_BINARY_NSIS OFF)
set(CPACK_SOURCE_ZIP ON)
set(CPACK_SOURCE_IGNORE_FILES "build/;.git/;.gitignore;lib/;mingw/;mingw64/;mingw-include/;lsb/;swp$;~$;${CPACK_SOURCE_IGNORE_FILES}")

if(CMAKE_C_COMPILER MATCHES ".*w64.*")
  set(CPACK_SYSTEM_NAME "win64")
endif()

include(CPack)
