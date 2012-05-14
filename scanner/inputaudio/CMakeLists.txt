cmake_minimum_required(VERSION 2.6.4)

if(USE_GLIB)
  include_directories(.)
  include_directories(SYSTEM ${GLIB_INCLUDE_DIRS})

  add_library(input input)
  target_link_libraries(input ${GMODULE_LIBRARY})

  if(WIN32 AND NOT "${CMAKE_SHARED_MODULE_PREFIX}")
    set(CMAKE_SHARED_MODULE_PREFIX "lib")
  endif()

  if(USE_SNDFILE)
    add_subdirectory(sndfile)
  endif()

  if(USE_MPG123)
    add_subdirectory(mpg123)
  endif()

  if(USE_MPCDEC)
    add_subdirectory(mpcdec)
  endif()

  if(USE_AVFORMAT)
    add_subdirectory(ffmpeg)
  endif()

  if(USE_GSTREAMER)
    add_subdirectory(gstreamer)
  endif()
endif()
