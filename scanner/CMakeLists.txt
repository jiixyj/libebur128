cmake_minimum_required(VERSION 2.6.4)

if(GLIB_FOUND)
  list(APPEND SCANNER_SRCS scanner.c nproc.c input.c)
  if(USE_TAGLIB)
    list(APPEND SCANNER_SRCS rgtag.cpp)
  endif()
  # Enable globbing for MinGW builds
  if(WIN32)
    list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".o")
    find_library(CRT_GLOB CRT_glob.o)
    list(REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES ".o")
    if(CRT_GLOB)
      message(STATUS "Linking against CRT_glob.o to enable globbing...")
      list(APPEND SCANNER_SRCS ${CRT_GLOB})
    endif()
  endif()

  add_executable(r128-scanner ${SCANNER_SRCS})
  target_link_libraries(r128-scanner ebur128 ${GLIB_LIBRARIES} ${TAGLIB_LIBRARIES})

  if(SNDFILE_FOUND)
    add_executable(r128-minimal-example minimal_example.c)
    target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})

    add_executable(r128-test-library tests.c)
    target_link_libraries(r128-test-library ebur128 ${SNDFILE_LIBRARIES})

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
    if(LSB_COMPILER_FOUND)
      set(FFMPEG_INPUT input_ffmpeg${FFMPEG_VERSION})
    else()
      set(FFMPEG_INPUT input_ffmpeg)
    endif()
    add_library(${FFMPEG_INPUT} MODULE input_ffmpeg.c)
    target_link_libraries(${FFMPEG_INPUT} ebur128 ${FFMPEG_LIBRARIES} ${GLIB_LIBRARIES})
    set_target_properties(${FFMPEG_INPUT} PROPERTIES COMPILE_FLAGS "-std=c99")
  endif()
endif()

if(GLIB_FOUND AND SNDFILE_FOUND AND MPG123_FOUND AND MPCDEC_FOUND AND
   FFMPEG_FOUND AND TAGLIB_FOUND)
  install(TARGETS r128-scanner ${FFMPEG_INPUT} input_sndfile input_mpg123 input_musepack
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

  set(INSTALL_READY TRUE PARENT_SCOPE)
endif()
