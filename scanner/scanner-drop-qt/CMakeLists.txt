include(${QT_USE_FILE})
qt4_automoc(scanner-drop-qt.cpp)
add_executable(loudness-drop-qt scanner-drop-qt)

include_directories(SYSTEM ${QT_INCLUDES})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(loudness-drop-qt scanner-tag ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTSVG_LIBRARY})

install(TARGETS loudness-drop-qt
        RUNTIME DESTINATION ${FOLDER_BIN}
        LIBRARY DESTINATION ${FOLDER_PLUGIN})
