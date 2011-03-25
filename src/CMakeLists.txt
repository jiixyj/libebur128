include_directories(queue)

add_library(ebur128 ebur128.c)
if(NOT WIN32)
  target_link_libraries(ebur128 m)
endif()

add_subdirectory(speex_resample)
include_directories(speex_resample)
target_link_libraries(ebur128 speex_resample)

if(GLIB_FOUND AND GTHREAD_FOUND AND INTL_FOUND)
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

  set(SCANNER_LIBS ${SCANNER_LIBS} ebur128 nproc)

  if(SNDFILE_FOUND)
    add_library(sndfile_input input_sndfile.c)
    target_link_libraries(sndfile_input ${SNDFILE_LIBRARIES})

    add_executable(r128-minimal-example minimal_example.c)
    target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})

    add_executable(r128-sndfile scanner.c)
    target_link_libraries(r128-sndfile ${SCANNER_LIBS} sndfile_input ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})

    add_executable(r128-test-library tests.c)
    target_link_libraries(r128-test-library ebur128 ${SNDFILE_LIBRARIES})
  endif()

  if(MPG123_FOUND)
    add_library(mpg123_input input_mpg123.c)
    target_link_libraries(mpg123_input ${MPG123_LIBRARIES})

    add_executable(r128-mpg123 scanner.c)
    target_link_libraries(r128-mpg123 ${SCANNER_LIBS} mpg123_input ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})
  endif()

  if(MPCDEC_FOUND)
    add_library(musepack_input input_mpcdec.c)
    target_link_libraries(musepack_input ${MPCDEC_LIBRARY})

    add_executable(r128-musepack scanner.c)
    target_link_libraries(r128-musepack ${SCANNER_LIBS} musepack_input ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})
  endif()

  if(FLAG_STD_C99 AND LIBAVFORMAT_FOUND AND LIBAVCODEC_FOUND AND LIBAVUTIL_FOUND)
    if(LSB_COMPILER_FOUND)
      set(R128_FFMPEG r128-ffmpeg-${FFMPEG_VERSION})
      set(FFMPEG_INPUT ffmpeg_input_${FFMPEG_VERSION})
    else()
      set(R128_FFMPEG r128-ffmpeg)
      set(FFMPEG_INPUT ffmpeg_input)
    endif()
    add_library(${FFMPEG_INPUT} input_ffmpeg.c)
    target_link_libraries(${FFMPEG_INPUT} ${LIBAVCODEC_LIBRARIES} ${LIBAVFORMAT_LIBRARIES} ${LIBAVUTIL_LIBRARIES})
    set_target_properties(${FFMPEG_INPUT} PROPERTIES COMPILE_FLAGS "-std=c99")

    add_executable(${R128_FFMPEG} scanner.c)
    target_link_libraries(${R128_FFMPEG} ${SCANNER_LIBS} ${FFMPEG_INPUT} ${GLIB_LIBRARIES} ${GTHREAD_LIBRARIES})
  endif()
endif()

if(GLIB_FOUND AND GTHREAD_FOUND AND INTL_FOUND AND SNDFILE_FOUND AND
   MPG123_FOUND AND MPCDEC_FOUND AND LIBAVFORMAT_FOUND AND LIBAVCODEC_FOUND AND
   LIBAVUTIL_FOUND AND TAGLIB_FOUND)
  install(TARGETS r128-sndfile ${R128_FFMPEG} r128-mpg123 r128-musepack
          RUNTIME DESTINATION .
          ARCHIVE DESTINATION lib)
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
            ${GCC_SJLJ_LIBRARY}
            DESTINATION .)
  endif()

  set(INSTALL_READY TRUE PARENT_SCOPE)
endif()
