include_directories(SYSTEM ${AVFORMAT_INCLUDE_DIRS})
add_library(input_ffmpeg MODULE input_ffmpeg.c mp3_padding.c mp4_padding.c ../input_helper.c)
target_link_libraries(input_ffmpeg ${AVFORMAT_LIBRARIES} ${GLIB_LIBRARY})
install(TARGETS input_ffmpeg LIBRARY DESTINATION ${FOLDER_PLUGIN})
