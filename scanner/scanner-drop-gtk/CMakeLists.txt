if(TARGET scanner-tag)
  find_package(GTK2 COMPONENTS gtk)
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(RSVG2 librsvg-2.0)
  endif()

  if(GTK2_FOUND AND RSVG2_FOUND)
    add_definitions(${RSVG2_CFLAGS_OTHER})
    include_directories(SYSTEM ${GTK2_INCLUDE_DIRS} ${RSVG2_INCLUDE_DIRS})
    include_directories(${LOGO_INCLUDE_DIR})

    add_executable(loudness-drop-gtk scanner-drop)

    target_link_libraries(loudness-drop-gtk
            scanner-tag filetree input
            ${GTK2_LIBRARIES}
            ${GLIB20_LIBRARIES}
            ${GTHREAD20_LIBRARIES}
            ${RSVG2_LIBRARIES})
  endif()
endif()
