cmake_minimum_required(VERSION 2.8)

set(SCANNERCOMMON_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/scanner-common
    ${CMAKE_CURRENT_SOURCE_DIR}/scanner-tag
    ${CMAKE_CURRENT_SOURCE_DIR}/filetree
    ${CMAKE_CURRENT_SOURCE_DIR}/inputaudio
    ${EBUR128_INCLUDE_DIR})

add_subdirectory(filetree)
add_subdirectory(inputaudio)

include(utils)

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(GLIB20    glib-2.0)
  pkg_check_modules(GTHREAD20 gthread-2.0)
  pkg_check_modules(SPEEXDSP  speexdsp)
endif()
find_package(SNDFILE)

set(SUMMARY_GLIB20_FOUND    ${GLIB20_FOUND}    CACHE INTERNAL "")
set(SUMMARY_GTHREAD20_FOUND ${GTHREAD20_FOUND} CACHE INTERNAL "")
set(SUMMARY_SNDFILE_FOUND   ${SNDFILE_FOUND}   CACHE INTERNAL "")
set(SUMMARY_SPEEXDSP_FOUND  ${SPEEXDSP_FOUND}  CACHE INTERNAL "")

if(GLIB20_FOUND AND GTHREAD20_FOUND AND NOT DISABLE_GLIB20)
  include_directories(${SCANNERCOMMON_INCLUDE_DIRS})

  add_subdirectory(scanner-common)
  add_subdirectory(scanner-tag)


  include_directories(SYSTEM ${GLIB20_INCLUDE_DIRS} ${GTHREAD20_INCLUDE_DIRS})
  link_directories(          ${GLIB20_LIBRARY_DIRS} ${GTHREAD20_LIBRARY_DIRS})
  add_definitions(           ${GLIB20_CFLAGS_OTHER} ${GTHREAD20_CFLAGS_OTHER})


  set(LOGO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  add_subdirectory(scanner-drop-gtk)
  add_subdirectory(scanner-drop-qt)


  add_library(scanner-lib scanner-scan scanner-dump)
  target_link_libraries(scanner-lib scanner-common ebur128)

  add_executable(loudness scanner)
  target_link_libraries(loudness
        scanner-lib filetree input
        ${GLIB20_LIBRARIES} ${GTHREAD20_LIBRARIES})



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
