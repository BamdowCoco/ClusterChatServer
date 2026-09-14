# Qt 客户端 (ChatClientQt)
# 依赖 Qt5 Widgets + Network，采用信号槽事件驱动模型

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

set(SRC_LIST
    main.cpp
    networkclient.cpp
    loginwidget.cpp
    mainwindow.cpp
)

add_executable(ChatClientQt ${SRC_LIST})
target_link_libraries(ChatClientQt Qt5::Widgets Qt5::Network)
