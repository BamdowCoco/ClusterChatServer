#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <functional>
#include <mutex>
#include <queue>
#include <string>

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接redis服务器
    bool connect();

    // 向redis指定的通道channel 发布消息
    bool publish(int channel, std::string message);

    // 向redis指定的通道 订阅消息
    bool subscribe(int channel);

    // 向redis指定的通道 取消订阅消息
    bool unsubscribe(int channel);

    // 在独立线程中接收订阅通道中的消息
    void observeChannelMessage();

    // 初始化向业务层上报通道消息的回调对象
    void initNotifyHandler(std::function<void(int, std::string)> fn);

private:
    const char* REDIS_IP = "127.0.0.1";
    const int REDIS_PORT = 6379;
    
    // hiredis同步上下文对象 负责publish消息
    redisContext* _publishContext;

    // hiredis同步上下文对象 负责subscribe消息
    redisContext* _subscribeContext;

    // 回调操作 收到订阅消息 上报给service层
    std::function<void(int, std::string)> _notifyMessageHandler;

    // hiredis context 非线程安全, 多线程并发 publish 需加锁保护
    std::mutex _publishMutex;

    // 订阅命令队列: 业务线程(sub-reactor)入队, 订阅线程独占 _subscribeContext 发送
    // (避免 redisGetReply 内部 redisBufferWrite 与 redisAppendCommand 并发操作 obuf 导致崩溃)
    std::queue<std::string> _subCommands;
    std::mutex _subCmdMutex;
    int _subCmdEventFd; // eventfd, 唤醒订阅线程处理新命令
};

#endif