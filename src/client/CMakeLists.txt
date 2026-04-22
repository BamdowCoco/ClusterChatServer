aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
# list(REMOVE_ITEM SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/receiverThread.cpp)

add_executable(ChatClient ${SRC_LIST})
target_link_libraries(ChatClient pthread)