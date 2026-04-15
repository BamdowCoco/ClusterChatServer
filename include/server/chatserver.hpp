#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
#include <functional>
#include "json.hpp"

using muduo::net::EventLoop;
using muduo::net::TcpServer;
using muduo::net::InetAddress;
using muduo::net::TcpConnectionPtr;
using muduo::net::Buffer;
using muduo::Timestamp;

class ChatServer
{
public:
    // 初始化聊天服务器
    ChatServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const std::string &nameArg);

    // 启动服务
    void start();

private:
    // 上报链接相关信息回调函数
    // std::function<void (const muduo::net::TcpConnectionPtr&)> ConnectionCallback;
    void onConnection(const TcpConnectionPtr &conn);

    // 上报读写事件信息回调函数
    /* std::function<void (const TcpConnectionPtr&,
                            Buffer*,
                            Timestamp)> MessageCallback;*/
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buffer,
                   Timestamp timestp);
    TcpServer _server;
    EventLoop *_loop;
};

#endif // CHATSERVER_H