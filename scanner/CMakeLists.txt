cmake_minimum_required(VERSION 2.6.4)


if(USE_GLIB)
  add_subdirectory(filetree)
  add_subdirectory(inputaudio)

  include_directories(filetree inputaudio)
  include_directories(SYSTEM ${GLIB_INCLUDE_DIRS})

  list(APPEND SCANNER_SRCS parse_args nproc scanner-scan scanner-dump scanner-common)

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

  add_library(scanner-common ${SCANNER_SRCS})
  target_link_libraries(scanner-common ebur128 input filetree ${GLIB_LIBRARY} ${GTHREAD_LIBRARY})
  if(USE_TAGLIB)
    add_library(scanner-common-tag scanner-tag rgtag)
    set_property(TARGET scanner-common-tag APPEND PROPERTY
            INCLUDE_DIRECTORIES ${TAGLIB_INCLUDE_DIRS})
    target_link_libraries(scanner-common-tag scanner-common)
    target_link_libraries(scanner-common scanner-common-tag ${TAGLIB_LIBRARIES})
  endif()
  if(USE_SNDFILE)
    target_link_libraries(scanner-common ${SNDFILE_LIBRARIES})
  endif()

  add_executable(loudness scanner)
  target_link_libraries(loudness scanner-common)
  if(M_LIBRARY)
    target_link_libraries(loudness ${M_LIBRARY})
  endif()

  install(TARGETS loudness
          RUNTIME DESTINATION ${FOLDER_BIN}
          LIBRARY DESTINATION ${FOLDER_PLUGIN})

  if(USE_RSVG2 AND USE_GTK2 AND USE_TAGLIB)
    add_subdirectory(scanner-drop-gtk)
  endif()
  if(USE_QT AND USE_TAGLIB)
    add_subdirectory(scanner-drop-qt)
  endif()
endif()
