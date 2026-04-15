#include "chatserver.hpp"
#include "chatservice.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <csignal>
using namespace std;
using namespace muduo;
using namespace muduo::net;

// 处理服务器接收SIGINT关闭，重置user状态
void resetHandler(int)
{
    ChatService::instance().reset();
    exit(0);
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(1);
    }

    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 捕获信号SIGINT 在服务器结束前进行指定操作
    // 回顾
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = resetHandler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, nullptr);

    EventLoop loop;
    InetAddress listenAddr(ip, port);
    string name = "ChatServer";
    ChatServer server(&loop, listenAddr, name);
    
    server.start();
    loop.loop();
    return 0;
}