aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)

add_executable(ChatClient ${SRC_LIST})
target_link_libraries(ChatClient pthread)