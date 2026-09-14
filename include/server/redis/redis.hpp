#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <functional>
#include <mutex>
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

    // 保护 _subscribeContext 写操作(subscribe/unsubscribe)的互斥锁
    std::mutex _subscribeMutex;
};

#endif