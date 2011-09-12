cmake_minimum_required(VERSION 2.6.4)

if(GLIB_FOUND)
  list(APPEND SCANNER_SRCS scanner.c nproc.c)
  if(USE_TAGLIB)
    include_directories(SYSTEM ${TAGLIB_INCLUDE_DIRS})
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
  target_link_libraries(r128-scanner ebur128 input ${GLIB_LIBRARIES} ${TAGLIB_LIBRARIES})

  if(SNDFILE_FOUND)
    add_executable(r128-minimal-example minimal_example.c)
    target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})
  endif()

  install(TARGETS r128-scanner
          RUNTIME DESTINATION ${FOLDER_BIN}
          LIBRARY DESTINATION ${FOLDER_PLUGIN})
endif()
