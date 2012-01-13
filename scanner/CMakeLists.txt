cmake_minimum_required(VERSION 2.6.4)

if(GLIB_FOUND)
  add_subdirectory(filetree)
  add_subdirectory(inputaudio)
  include_directories(filetree inputaudio)

  include_directories(SYSTEM ${GLIB_INCLUDE_DIRS})

  add_library(parse_args parse_args)

  list(APPEND SCANNER_SRCS scanner.c nproc.c
                           scanner-scan.c scanner-dump.c
                           scanner-common.c)
  if(TAGLIB_FOUND)
    include_directories(SYSTEM ${TAGLIB_INCLUDE_DIRS})
    list(APPEND SCANNER_SRCS scanner-tag.c rgtag.cpp)
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

  add_executable(loudness ${SCANNER_SRCS})
  target_link_libraries(loudness ebur128 input filetree parse_args ${GLIB_LIBRARY} ${GTHREAD_LIBRARY})
  if(TAGLIB_FOUND)
    target_link_libraries(loudness ${TAGLIB_LIBRARIES})
  endif()
  install(TARGETS loudness
          RUNTIME DESTINATION ${FOLDER_BIN}
          LIBRARY DESTINATION ${FOLDER_PLUGIN})

  if(GTK2_FOUND AND RSVG2_FOUND AND TAGLIB_FOUND)
    include_directories(SYSTEM ${GTK2_INCLUDE_DIRS})
    include_directories(SYSTEM ${RSVG2_INCLUDE_DIRS})
    add_executable(loudness-drop-gtk scanner-drop scanner-tag scanner-common nproc rgtag.cpp)
    # we don't need pango and atk
    foreach(lib ${GTK2_LIBRARIES})
        if(NOT "${lib}" MATCHES ".*pango.*" AND NOT "${lib}" MATCHES ".*atk.*")
            list(APPEND MY_GTK2_LIBRARIES ${lib})
        endif()
    endforeach()
    target_link_libraries(loudness-drop-gtk ebur128 input filetree parse_args ${RSVG2_LIBRARIES} ${MY_GTK2_LIBRARIES} ${TAGLIB_LIBRARIES})
    install(TARGETS loudness-drop-gtk
            RUNTIME DESTINATION ${FOLDER_BIN}
            LIBRARY DESTINATION ${FOLDER_PLUGIN})
  endif()

  if(QT4_FOUND AND TAGLIB_FOUND)
    include(${QT_USE_FILE})
    include_directories(SYSTEM ${QT_INCLUDES})
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    qt4_automoc(scanner-drop-qt.cpp)
    add_executable(loudness-drop-qt scanner-drop-qt scanner-tag scanner-common nproc rgtag.cpp)
    target_link_libraries(loudness-drop-qt ebur128 input filetree parse_args ${GLIB_LIBRARY} ${GTHREAD_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTSVG_LIBRARY} ${TAGLIB_LIBRARIES})
    install(TARGETS loudness-drop-qt
            RUNTIME DESTINATION ${FOLDER_BIN}
            LIBRARY DESTINATION ${FOLDER_PLUGIN})
  endif()

  if(SNDFILE_FOUND)
    include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})
    add_executable(r128-minimal-example minimal_example.c)
    target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})
  endif()

endif()
