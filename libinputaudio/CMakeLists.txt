cmake_minimum_required(VERSION 2.6.4)

if(GLIB_FOUND)
  add_library(input input)

  if(WIN32 AND NOT "${CMAKE_SHARED_MODULE_PREFIX}")
    set(CMAKE_SHARED_MODULE_PREFIX "lib")
  endif()

  if(SNDFILE_FOUND)
    add_library(input_sndfile MODULE input_sndfile.c)
    target_link_libraries(input_sndfile ebur128 ${SNDFILE_LIBRARIES})
  endif()

  if(MPG123_FOUND)
    add_library(input_mpg123 MODULE input_mpg123.c)
    target_link_libraries(input_mpg123 ${MPG123_LIBRARIES})
  endif()

  if(MPCDEC_FOUND)
    add_library(input_musepack MODULE input_mpcdec.c)
    target_link_libraries(input_musepack ${MPCDEC_LIBRARY})
  endif()

  if(FLAG_STD_C99 AND FFMPEG_FOUND)
    add_library(input_ffmpeg MODULE input_ffmpeg.c)
    target_link_libraries(input_ffmpeg ebur128 ${FFMPEG_LIBRARIES} ${GLIB_LIBRARIES})
    set_target_properties(input_ffmpeg PROPERTIES COMPILE_FLAGS "-std=c99")
  endif()

  if(SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND FFMPEG_FOUND)
    install(TARGETS input_ffmpeg input_sndfile input_mpg123 input_musepack
            RUNTIME DESTINATION ${FOLDER_BIN}
            LIBRARY DESTINATION ${FOLDER_PLUGIN})
    if(LSB_COMPILER_FOUND)
      install(PROGRAMS ${EXECUTABLE_OUTPUT_PATH}/libinput_ffmpeg0.5.2.so
                       ${EXECUTABLE_OUTPUT_PATH}/libinput_ffmpeg0.6.2.so DESTINATION ${FOLDER_PLUGIN})
    endif()
    if(WIN32 AND FLAT_INSTALL)
      get_filename_component(REAL_AVC ${LIBAVCODEC_LIBRARY} REALPATH)
      get_filename_component(REAL_AVF ${LIBAVFORMAT_LIBRARY} REALPATH)
      get_filename_component(REAL_AVU ${LIBAVUTIL_LIBRARY} REALPATH)
      get_filename_component(NAME_AVC ${LIBAVCODEC_LIBRARY} NAME)
      get_filename_component(NAME_AVF ${LIBAVFORMAT_LIBRARY} NAME)
      get_filename_component(NAME_AVU ${LIBAVUTIL_LIBRARY} NAME)
      install(TARGETS rgtag RUNTIME DESTINATION .
              PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
      install(FILES ${REAL_AVC} DESTINATION . RENAME ${NAME_AVC})
      install(FILES ${REAL_AVF} DESTINATION . RENAME ${NAME_AVF})
      install(FILES ${REAL_AVU} DESTINATION . RENAME ${NAME_AVU})
      install(FILES
              ${MPG123_LIBRARIES}
              ${SNDFILE_LIBRARIES}
              ${GLIB_LIBRARIES}
              ${TAGLIB_LIBRARIES}
              ${GCC_SJLJ}
              ${STDCPP}
              DESTINATION .)
    endif()
  endif()
endif()
