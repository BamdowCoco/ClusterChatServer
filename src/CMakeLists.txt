# 配置子目录
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/server)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/client)

# Qt 客户端（可选）：未安装 Qt 时不影响原有终端客户端/服务端构建
find_package(Qt5 QUIET COMPONENTS Widgets Network)
if(Qt5_FOUND)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/qtclient)
endif()