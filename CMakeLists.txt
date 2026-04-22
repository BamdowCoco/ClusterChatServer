cmake_minimum_required(VERSION 3.0)

project(chat)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 启用调试信息
set(CMAKE_BUILD_TYPE "Debug")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUGS)
endif()

# 配置最终可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 配置头文件搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/server
    ${PROJECT_SOURCE_DIR}/include/server/database
    ${PROJECT_SOURCE_DIR}/include/server/model
    ${PROJECT_SOURCE_DIR}/include/server/redis
    ${PROJECT_SOURCE_DIR}/include/client
)

# 配置第三方搜索路径
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/thirdparty
)

# 配置子目录
add_subdirectory(${PROJECT_SOURCE_DIR}/src)