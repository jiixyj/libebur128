add_executable(loudness-drop-gtk scanner-drop)

include_directories(SYSTEM ${GTK2_INCLUDE_DIRS})
include_directories(SYSTEM ${RSVG2_INCLUDE_DIRS})

# we don't need pango and atk
foreach(lib ${GTK2_LIBRARIES})
    if(NOT "${lib}" MATCHES ".*pango.*" AND
       NOT "${lib}" MATCHES ".*atk.*" AND
       NOT "${lib}" MATCHES ".*gdk_pixbuf-2.0.*" AND
       NOT "${lib}" MATCHES ".*freetype.*")
        list(APPEND MY_GTK2_LIBRARIES ${lib})
    endif()
endforeach()

target_link_libraries(loudness-drop-gtk scanner-tag ${RSVG2_LIBRARIES} ${MY_GTK2_LIBRARIES})

install(TARGETS loudness-drop-gtk
        RUNTIME DESTINATION ${FOLDER_BIN}
        LIBRARY DESTINATION ${FOLDER_PLUGIN})
