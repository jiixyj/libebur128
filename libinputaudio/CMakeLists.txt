cmake_minimum_required(VERSION 2.6.4)

if(GSTREAMER_INPUT_STATIC)
  add_definitions(-DGSTREAMER_INPUT_STATIC)
endif()

if(GLIB_FOUND)
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
    add_library(input_sndfile MODULE input_sndfile.c)
    target_link_libraries(input_sndfile ebur128 ${SNDFILE_LIBRARIES} ${INPUT_HELPERS})
  endif()

  if(MPG123_FOUND)
    add_library(input_mpg123 MODULE input_mpg123.c)
    target_link_libraries(input_mpg123 ${MPG123_LIBRARIES})
  endif()

  if(MPCDEC_FOUND)
    add_library(input_musepack MODULE input_mpcdec.c)
    target_link_libraries(input_musepack ${MPCDEC_LIBRARY} ${INPUT_HELPERS})
  endif()

  if(FFMPEG_FOUND)
    add_library(input_ffmpeg MODULE input_ffmpeg.c)
    target_link_libraries(input_ffmpeg ebur128 ${FFMPEG_LIBRARIES} ${GLIB_LIBRARIES})
  endif()

  if(GSTREAMER_FOUND)
    if(GSTREAMER_INPUT_STATIC)
      add_library(input_gstreamer input_gstreamer.c)
      target_link_libraries(input input_gstreamer)
    else()
      add_library(input_gstreamer MODULE input_gstreamer.c)
    endif()
    target_link_libraries(input_gstreamer ebur128 ${GLIB_LIBRARIES} ${GTK2_GOBJECT_LIBRARY} ${GSTREAMER_LIBRARIES})
  endif()

  if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND FFMPEG_FOUND)
    install(TARGETS input_ffmpeg input_sndfile input_mpg123 input_musepack
            RUNTIME DESTINATION ${FOLDER_BIN}
            LIBRARY DESTINATION ${FOLDER_PLUGIN})
  endif()
endif()
