cmake_minimum_required(VERSION 2.8)

set(SCANNERCOMMON_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/scanner-common
    ${CMAKE_CURRENT_SOURCE_DIR}/scanner-tag
    ${CMAKE_CURRENT_SOURCE_DIR}/filetree
    ${CMAKE_CURRENT_SOURCE_DIR}/inputaudio
    ${EBUR128_INCLUDE_DIR})

add_subdirectory(filetree)
add_subdirectory(inputaudio)
add_subdirectory(scanner-tag)

include(utils)

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(GLIB20    glib-2.0)
  pkg_check_modules(GTHREAD20 gthread-2.0)
  pkg_check_modules(SPEEXDSP  speexdsp)

  to_space_list(GLIB20_CFLAGS)
  to_space_list(GLIB20_LDFLAGS)

  to_space_list(GTHREAD20_CFLAGS)
  to_space_list(GTHREAD20_LDFLAGS)

  to_space_list(SPEEXDSP_CFLAGS)
  to_space_list(SPEEXDSP_LDFLAGS)
endif()
find_package(SNDFILE)

set(SUMMARY_GLIB20_FOUND    ${GLIB20_FOUND}    CACHE INTERNAL "")
set(SUMMARY_GTHREAD20_FOUND ${GTHREAD20_FOUND} CACHE INTERNAL "")
set(SUMMARY_SNDFILE_FOUND   ${SNDFILE_FOUND}   CACHE INTERNAL "")
set(SUMMARY_SPEEXDSP_FOUND  ${SPEEXDSP_FOUND}  CACHE INTERNAL "")

if(GLIB20_FOUND AND GTHREAD20_FOUND AND NOT DISABLE_GLIB20)

  add_subdirectory(scanner-common)
  include_directories(${SCANNERCOMMON_INCLUDE_DIRS})

  set(LOUDNESS_CFLAGS  "${GLIB20_CFLAGS}  ${GTHREAD20_CFLAGS}")
  set(LOUDNESS_LDFLAGS "${GLIB20_LDFLAGS} ${GTHREAD20_LDFLAGS} ${INPUT_LDFLAGS}")

  set(LOGO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  add_subdirectory(scanner-drop-gtk)
  add_subdirectory(scanner-drop-qt)


  add_library(scanner-lib scanner-scan scanner-dump)
  target_link_libraries(scanner-lib scanner-common ebur128)

  add_executable(loudness scanner)
  target_link_libraries(loudness scanner-lib filetree input)


  set_target_properties(loudness PROPERTIES
          COMPILE_FLAGS ${LOUDNESS_CFLAGS}
             LINK_FLAGS ${LOUDNESS_LDFLAGS})

  set_target_properties(scanner-lib PROPERTIES
          COMPILE_FLAGS ${LOUDNESS_CFLAGS})


  if(SNDFILE_FOUND AND NOT DISABLE_SNDFILE)
    set_property(TARGET loudness APPEND PROPERTY
          COMPILE_DEFINITIONS "USE_SNDFILE")
  endif()

  if(SPEEXDSP_FOUND AND NOT DISABLE_SPEEXDSP)
    add_definitions(-DUSE_SPEEX_RESAMPLER)
  endif()

  if(TARGET scanner-tag)
    target_link_libraries(loudness scanner-tag)
    set_property(TARGET loudness APPEND PROPERTY
          COMPILE_DEFINITIONS "USE_TAGLIB")
  endif()
endif()
