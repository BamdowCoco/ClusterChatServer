aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
# list(REMOVE_ITEM SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/receiverThread.cpp)
# aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../server/database SRC_LIST)
# aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../server/model SRC_LIST)

add_executable(ChatClient ${SRC_LIST})
target_link_libraries(ChatClient pthread)