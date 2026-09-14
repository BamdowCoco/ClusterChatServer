#include "chatserver.hpp"
#include "chatservice.hpp"

#include "json.hpp"
#include <cstdint>
#include <exception>
#include <functional>
#include <muduo/base/Logging.h>
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
    // 变长数据包分帧: [4字节长度头(网络字节序) + JSON 体]
    // 循环处理粘包(一个TCP段含多条消息) 与 半包(一条消息未收全, 等待更多数据)
    static const int32_t kMaxMsgLen = 1 << 20; // 单条消息最大长度 1MB, 防止非法长度
    while (buffer->readableBytes() >= sizeof(int32_t)) {
        int32_t len = buffer->peekInt32();
        // 长度合法性校验: 防止非法数据导致越界/死循环
        if (len <= 0 || static_cast<uint32_t>(len) > kMaxMsgLen) {
            LOG_ERROR << "invalid packet length: " << len;
            conn->shutdown();
            break;
        }
        // 半包: 数据体未收全, 不消费缓冲区, 等待下一次 onMessage 携带更多数据
        if (buffer->readableBytes() < sizeof(int32_t) + static_cast<size_t>(len)) {
            return;
        }
        // 消费长度头, 取出数据体
        buffer->retrieve(sizeof(int32_t));
        string body = buffer->retrieveAsString(static_cast<size_t>(len));

        // 数据反序列化
        json js;
        try {
            js = json::parse(body);
        } catch (const std::exception& e) {
            LOG_ERROR << "json parse error: " << e.what();
            continue;
        }

        // 目的: 将网络模块和业务模块完全解耦合
        // 实现方式: 通过js["msgid"]获取 => 业务handler
        auto msgHandler = ChatService::instance().getHandler(js["msgid"].get<int>());
        // 回调消息id绑定好的事件处理器，来处理业务
        msgHandler(conn, js, timestp);
    }
}
