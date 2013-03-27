if(TARGET scanner-tag)
  find_package(Qt4 COMPONENTS QtGui QtSvg)

  if(QT4_FOUND)
    include(${QT_USE_FILE})
    qt4_automoc(scanner-drop-qt.cpp)
    add_executable(loudness-drop-qt scanner-drop-qt)

    include_directories(${CMAKE_CURRENT_BINARY_DIR} ${LOGO_INCLUDE_DIR})
    target_link_libraries(loudness-drop-qt
            scanner-tag filetree input
            ${GLIB20_LIBRARIES}
            ${GTHREAD20_LIBRARIES}
            ${QT_QTCORE_LIBRARY}
            ${QT_QTGUI_LIBRARY}
            ${QT_QTSVG_LIBRARY})
  endif()
endif()
