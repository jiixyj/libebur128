cmake_minimum_required(VERSION 2.6.4)

if(USE_GLIB)
  add_subdirectory(filetree)
  add_subdirectory(inputaudio)

  include_directories(filetree inputaudio scanner-common scanner-tag)
  include_directories(SYSTEM ${GLIB_INCLUDE_DIRS})

  list(APPEND SCANNER_SRCS scanner-scan scanner-dump)

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

  add_library(scanner-lib ${SCANNER_SRCS})

  add_subdirectory(scanner-common)
  target_link_libraries(scanner-lib scanner-common)

  add_executable(loudness scanner)
  target_link_libraries(loudness scanner-lib)
  if(M_LIBRARY)
    target_link_libraries(loudness ${M_LIBRARY})
  endif()

  install(TARGETS loudness
          RUNTIME DESTINATION ${FOLDER_BIN}
          LIBRARY DESTINATION ${FOLDER_PLUGIN})

  if(USE_TAGLIB)
    add_subdirectory(scanner-tag)

    target_link_libraries(loudness scanner-tag)

    if(USE_RSVG2 AND USE_GTK2)
      add_subdirectory(scanner-drop-gtk)
    endif()

    if(USE_QT)
      add_subdirectory(scanner-drop-qt)
    endif()
  endif()
endif()
