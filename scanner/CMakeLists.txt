if(GLIB_FOUND AND GTHREAD_FOUND AND GMODULE_FOUND AND INTL_FOUND)
  if(USE_TAGLIB)
    if(WIN32)
      add_library(rgtag SHARED rgtag.cpp)
    else()
      add_library(rgtag rgtag.cpp)
    endif()
    target_link_libraries(rgtag ${TAGLIB_LIBRARIES})
    set(SCANNER_LIBS rgtag)
  endif()

  add_library(nproc nproc.c)
  add_library(input input.c)
  target_link_libraries(input ${GMODULE_LIBRARIES})

  set(SCANNER_SRCS scanner.c)
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

  set(SCANNER_LIBS ${SCANNER_LIBS} ebur128 input nproc)

  add_executable(r128-scanner ${SCANNER_SRCS})
  target_link_libraries(r128-scanner ${SCANNER_LIBS} ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})

  add_executable(r128-minimal-example minimal_example.c)
  target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})

  add_executable(r128-test-library tests.c)
  target_link_libraries(r128-test-library ebur128 ${SNDFILE_LIBRARIES})


  set(CMAKE_SHARED_MODULE_PREFIX "")

  if(SNDFILE_FOUND)
    add_library(input_sndfile MODULE input_sndfile.c)
    target_link_libraries(input_sndfile ${SNDFILE_LIBRARIES})
  endif()

  if(MPG123_FOUND)
    add_library(input_mpg123 MODULE input_mpg123.c)
    target_link_libraries(input_mpg123 ${MPG123_LIBRARIES})
  endif()

  if(MPCDEC_FOUND)
    add_library(input_musepack MODULE input_mpcdec.c)
    target_link_libraries(input_musepack ${MPCDEC_LIBRARY})
  endif()

  if(FLAG_STD_C99 AND LIBAVFORMAT_FOUND AND LIBAVCODEC_FOUND AND LIBAVUTIL_FOUND)
    if(LSB_COMPILER_FOUND)
      set(FFMPEG_INPUT input_ffmpeg${FFMPEG_VERSION})
    else()
      set(FFMPEG_INPUT input_ffmpeg)
    endif()
    add_library(${FFMPEG_INPUT} MODULE input_ffmpeg.c)
    target_link_libraries(${FFMPEG_INPUT} ${LIBAVCODEC_LIBRARIES} ${LIBAVFORMAT_LIBRARIES} ${LIBAVUTIL_LIBRARIES})
    set_target_properties(${FFMPEG_INPUT} PROPERTIES COMPILE_FLAGS "-std=c99")
  endif()
endif()

if(GLIB_FOUND AND GTHREAD_FOUND AND INTL_FOUND AND SNDFILE_FOUND AND
   MPG123_FOUND AND MPCDEC_FOUND AND LIBAVFORMAT_FOUND AND LIBAVCODEC_FOUND AND
   LIBAVUTIL_FOUND AND TAGLIB_FOUND)
  install(TARGETS r128-scanner ${FFMPEG_INPUT} input_sndfile input_mpg123 input_musepack
          RUNTIME DESTINATION .
          LIBRARY DESTINATION .)
  if(LSB_COMPILER_FOUND)
    install(PROGRAMS ${EXECUTABLE_OUTPUT_PATH}/r128-ffmpeg-0.5.2
                     ${EXECUTABLE_OUTPUT_PATH}/r128-ffmpeg-0.6.1 DESTINATION .)
  endif()
  if(WIN32)
    get_filename_component(REAL_AVC ${LIBAVCODEC_LIBRARIES} REALPATH)
    get_filename_component(REAL_AVF ${LIBAVFORMAT_LIBRARIES} REALPATH)
    get_filename_component(REAL_AVU ${LIBAVUTIL_LIBRARIES} REALPATH)
    get_filename_component(NAME_AVC ${LIBAVCODEC_LIBRARIES} NAME)
    get_filename_component(NAME_AVF ${LIBAVFORMAT_LIBRARIES} NAME)
    get_filename_component(NAME_AVU ${LIBAVUTIL_LIBRARIES} NAME)
    install(TARGETS rgtag RUNTIME DESTINATION .
            PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
    install(FILES ${REAL_AVC} DESTINATION . RENAME ${NAME_AVC})
    install(FILES ${REAL_AVF} DESTINATION . RENAME ${NAME_AVF})
    install(FILES ${REAL_AVU} DESTINATION . RENAME ${NAME_AVU})
    install(FILES
            ${MPG123_LIBRARIES}
            ${SNDFILE_LIBRARIES}
            ${INTL_LIBRARIES}
            ${GLIB_LIBRARIES}
            ${GTHREAD_LIBRARIES}
            ${TAGLIB_LIBRARIES}
            ${GCC_SJLJ}
            ${STDCPP}
            DESTINATION .)
  endif()

  set(INSTALL_READY TRUE PARENT_SCOPE)
endif()
