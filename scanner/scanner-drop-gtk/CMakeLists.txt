if(TARGET scanner-tag)
  find_package(GTK2 COMPONENTS gtk)
  find_package(PkgConfig)
  pkg_check_modules(RSVG2 librsvg-2.0)

  if(GTK2_FOUND AND RSVG2_FOUND)
    add_executable(loudness-drop-gtk scanner-drop)

    include_directories(SYSTEM ${GTK2_INCLUDE_DIRS})
    include_directories(${LOGO_INCLUDE_DIR})
    target_link_libraries(loudness-drop-gtk scanner-tag filetree input ${GTK2_LIBRARIES})

    to_space_list(RSVG2_CFLAGS)
    to_space_list(RSVG2_LDFLAGS)

    set_target_properties(loudness-drop-gtk PROPERTIES
            COMPILE_FLAGS "${LOUDNESS_CFLAGS}  ${RSVG2_CFLAGS}"
               LINK_FLAGS "${LOUDNESS_LDFLAGS} ${RSVG2_LDFLAGS}")
  endif()
endif()
