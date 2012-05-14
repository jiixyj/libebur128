include_directories(SYSTEM ${MPG123_INCLUDE_DIRS})
add_library(input_mpg123 MODULE input_mpg123.c)
target_link_libraries(input_mpg123 ${MPG123_LIBRARIES})
install(TARGETS input_mpg123 LIBRARY DESTINATION ${FOLDER_PLUGIN})
