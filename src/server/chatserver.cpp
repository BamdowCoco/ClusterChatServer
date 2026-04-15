#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
// #include <muduo/base/Logging.h>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;
using json = nlohmann::json;

// 初始化聊天服务器
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册处理连接回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
    // 注册处理读写事件回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
    // 设置sub reactors数目
    _server.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    _server.start();
}

// 上报链接相关信息回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开连接/异常退出
    if (!conn->connected()) {
        ChatService::instance().clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件信息回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp timestp)
{
    // size_t len = buffer->readableBytes();
    string buff = buffer->retrieveAllAsString();

    // LOG_INFO << "Received " << len << " bytes: " << buff;

    // 数据反序列化
    // 需要捕获json异常!
    json js = json::parse(buff);
    // 目的: 将网络模块和业务模块完全解耦合
    // 实现方式: 通过js["msgid"]获取 => 业务handler
    auto msgHandler = ChatService::instance().getHandler(js["msgid"].get<int>());
    // 回调消息id绑定好的事件处理器，来处理业务
    msgHandler(conn, js, timestp);
}
