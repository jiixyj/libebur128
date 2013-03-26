cmake_minimum_required(VERSION 2.8)

set(SCANNERCOMMON_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/scanner-common 
    ${CMAKE_CURRENT_SOURCE_DIR}/filetree
    ${CMAKE_CURRENT_SOURCE_DIR}/inputaudio
    ${EBUR128_INCLUDE_DIR})

add_subdirectory(filetree)
add_subdirectory(inputaudio)
add_subdirectory(scanner-tag)

include(utils)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB20 glib-2.0)
pkg_check_modules(GTHREAD20 gthread-2.0)

if(GLIB20_FOUND AND GTHREAD20_FOUND AND NOT DISABLE_GLIB20)
  add_subdirectory(scanner-common)

  list(APPEND SCANNER_SRCS scanner-scan scanner-dump)
  # Enable globbing for MinGW builds
  # if(WIN32)
  #   list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".o")
  #   find_library(CRT_GLOB CRT_glob.o)
  #   list(REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES ".o")
  #   if(CRT_GLOB)
  #     message(STATUS "Linking against CRT_glob.o to enable globbing...")
  #     list(APPEND SCANNER_SRCS ${CRT_GLOB})
  #   endif()
  # endif()

  to_space_list(GLIB20_CFLAGS)
  to_space_list(GLIB20_LDFLAGS)

  to_space_list(GTHREAD20_CFLAGS)
  to_space_list(GTHREAD20_LDFLAGS)

  include_directories(${SCANNERCOMMON_INCLUDE_DIRS})

  add_library(scanner-lib ${SCANNER_SRCS})
  target_link_libraries(scanner-lib scanner-common ebur128)
  set_target_properties(scanner-lib PROPERTIES
         COMPILE_FLAGS "${GLIB20_CFLAGS}  ${GTHREAD20_CFLAGS}")

  add_executable(loudness scanner)
  target_link_libraries(loudness scanner-lib filetree input)

  set(LOUDNESS_CFLAGS  "${GLIB20_CFLAGS}  ${GTHREAD20_CFLAGS}")
  set(LOUDNESS_LDFLAGS "${GLIB20_LDFLAGS} ${GTHREAD20_LDFLAGS} ${INPUT_LDFLAGS}")

  set_target_properties(loudness PROPERTIES
          COMPILE_FLAGS ${LOUDNESS_CFLAGS}
             LINK_FLAGS ${LOUDNESS_LDFLAGS})
  set_target_properties(scanner-lib PROPERTIES
          COMPILE_FLAGS ${LOUDNESS_CFLAGS}
             LINK_FLAGS ${LOUDNESS_LDFLAGS})

  if(TAGLIB_FOUND AND NOT DISABLE_TAGLIB)
    target_link_libraries(loudness scanner-tag)

    # if(USE_RSVG2 AND USE_GTK2)
    #   add_subdirectory(scanner-drop-gtk)
    # endif()

    # if(USE_QT)
    #   add_subdirectory(scanner-drop-qt)
    # endif()
  endif()
endif()
