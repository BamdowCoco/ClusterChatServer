#include "redis.hpp"
#include <cstdlib>
#include <hiredis/hiredis.h>
#include <iostream>

using namespace std;

Redis::Redis():_publishContext(nullptr), _subscribeContext(nullptr)
{
}

Redis::~Redis()
{
    if (!_publishContext) {
        redisFree(_publishContext);
    }
    if (!_subscribeContext) {
        redisFree(_subscribeContext);
    }
}

// 连接redis服务器
bool Redis::connect()
{
    // 负责publish消息的上下文连接(相当于一个redis-cli)
    _publishContext = redisConnect(REDIS_IP, REDIS_PORT);
    if (_publishContext == nullptr) {
        cerr << "failed to connect redis!" << endl;
        return false;
    }

    // 负责subscribe消息的上下文连接(相当于一个redis-cli)
    _subscribeContext = redisConnect(REDIS_IP, REDIS_PORT);
    if (_subscribeContext == nullptr) {
        cerr << "failed to connect redis!" << endl;
        return false;
    }

    // 在单独的线程中 监听通道上的事件 有消息上报给业务层
    thread t([&]() {
        observeChannelMessage();
    });
    t.detach();

    cout << "connect redis-server success!" << endl;

    return true;
}

// 向redis指定的通道channel 发布消息
bool Redis::publish(int channel, std::string message)
{
    redisReply* reply = (redisReply*)redisCommand(_publishContext, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr) {
        cerr << "failed to publish! channel:" << channel << " message:" << message << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道 订阅消息
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里发生的消息 这里只进行订阅通道 并不监听通道消息
    // 通道消息的监听接收 专门在observerChannelMessage函数中 使用独立线程进行

    // 只负责发送命令 不阻塞接收redis server响应消息
    if (REDIS_ERR == redisAppendCommand(_subscribeContext, "SUBSCRIBE %d", channel)) {
        cerr << "failed to subscribe!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区 直至缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while (done == 0) {
        if (REDIS_ERR == redisBufferWrite(_subscribeContext, &done)) {
            cerr << "failed to subscribe!" << endl;
            return false;
        }
    }

    // 无需 redisGetReply 阻塞等待响应

    return true;
}

// 向redis指定的通道 取消订阅消息
bool Redis::unsubscribe(int channel)
{
    // 只负责发送命令 不阻塞接收redis server响应消息
    if (REDIS_ERR == redisAppendCommand(_subscribeContext, "UNSUBSCRIBE %d", channel)) {
        cerr << "failed to unsubscribe!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区 直至缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while (done == 0) {
        if (REDIS_ERR == redisBufferWrite(_subscribeContext, &done)) {
            cerr << "failed to unsubscribe!" << endl;
            return false;
        }
    }

    // 交给独立线程去 redisGetReply 阻塞等待响应

    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observeChannelMessage()
{
    redisReply* reply = nullptr;
    while (REDIS_OK == redisGetReply(_subscribeContext, (void**)&reply)) {
        // 订阅收到的消息是一个 三元组
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr) {
            // 给业务层上报通道上发生的消息
            _notifyMessageHandler(atoi(reply->element[1]->str), reply->element[2]->str);
            freeReplyObject(reply);
        }
    }
    cerr << "oberverChannelMessage quit!" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::initNotifyHandler(std::function<void(int, std::string)> fn)
{
    _notifyMessageHandler = fn;
}
