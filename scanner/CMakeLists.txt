cmake_minimum_required(VERSION 2.6.4)

if(GLIB_FOUND)
  add_library(parse_global_args parse_global_args)

  list(APPEND SCANNER_SRCS scanner-new.c nproc.c scanner-scan.c)
  if(TAGLIB_FOUND)
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

  add_executable(loudness ${SCANNER_SRCS})
  target_link_libraries(loudness ebur128 input filetree parse_global_args ${GLIB_LIBRARIES})
  if(TAGLIB_FOUND)
    target_link_libraries(loudness ${TAGLIB_LIBRARIES})
  endif()

  if(SNDFILE_FOUND)
    add_executable(r128-minimal-example minimal_example.c)
    target_link_libraries(r128-minimal-example ebur128 ${SNDFILE_LIBRARIES})
  endif()

  install(TARGETS loudness
          RUNTIME DESTINATION ${FOLDER_BIN}
          LIBRARY DESTINATION ${FOLDER_PLUGIN})
endif()
