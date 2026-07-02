aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/database SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/model SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/redis SRC_LIST)

add_executable(ChatServer ${SRC_LIST})
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)