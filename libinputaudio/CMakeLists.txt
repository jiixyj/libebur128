cmake_minimum_required(VERSION 2.6.4)

if(GLIB_FOUND)
  include_directories(SYSTEM ${GLIB_INCLUDE_DIRS})

  add_library(input input)

  if(WIN32)
    add_library(input_win32 input_win32)
    target_link_libraries(input_win32 ${GLIB_LIBRARIES})
    set(INPUT_HELPERS input_win32)
  endif()

  if(WIN32 AND NOT "${CMAKE_SHARED_MODULE_PREFIX}")
    set(CMAKE_SHARED_MODULE_PREFIX "lib")
  endif()

  if(SNDFILE_FOUND)
    include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})
    add_library(input_sndfile MODULE input_sndfile.c)
    target_link_libraries(input_sndfile ${SNDFILE_LIBRARIES} ${INPUT_HELPERS})
  endif()

  if(MPG123_FOUND)
    include_directories(SYSTEM ${MPG123_INCLUDE_DIRS})
    add_library(input_mpg123 MODULE input_mpg123.c)
    target_link_libraries(input_mpg123 ${MPG123_LIBRARIES})
  endif()

  if(MPCDEC_FOUND)
    include_directories(SYSTEM ${MPCDEC_INCLUDE_DIRS})
    add_library(input_musepack MODULE input_mpcdec.c)
    target_link_libraries(input_musepack ${MPCDEC_LIBRARY} ${INPUT_HELPERS})
  endif()

  if(FFMPEG_FOUND)
    include_directories(SYSTEM ${FFMPEG_INCLUDE_DIRS})
    add_library(input_ffmpeg MODULE input_ffmpeg.c)
    target_link_libraries(input_ffmpeg ${FFMPEG_LIBRARIES} ${GLIB_LIBRARIES})
  endif()

  if(GSTREAMER_FOUND)
    include_directories(SYSTEM ${GSTREAMER_INCLUDE_DIRS})
    if(GSTREAMER_INPUT_STATIC)
      add_library(input_gstreamer input_gstreamer.c)
      target_link_libraries(input input_gstreamer)
      set_target_properties(input input_gstreamer PROPERTIES COMPILE_DEFINITIONS GSTREAMER_INPUT_STATIC)
    else()
      add_library(input_gstreamer MODULE input_gstreamer.c)
    endif()
    set_target_properties(input_gstreamer PROPERTIES COMPILE_FLAGS "${GSTREAMER_CFLAGS}")
    target_link_libraries(input_gstreamer ${GSTREAMER_LIBRARIES})
  endif()

  if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND FFMPEG_FOUND)
    install(TARGETS input_ffmpeg input_sndfile input_mpg123 input_musepack
            RUNTIME DESTINATION ${FOLDER_BIN}
            LIBRARY DESTINATION ${FOLDER_PLUGIN})
  endif()
endif()
