aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/database DATABASE_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/model MODEL_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/redis REDIS_LIST)

add_executable(ChatServer ${SRC_LIST} ${DATABASE_LIST} ${MODEL_LIST} ${REDIS_LIST})
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)