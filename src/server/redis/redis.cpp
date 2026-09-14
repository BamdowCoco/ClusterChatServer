#include "redis.hpp"
#include <cstdlib>
#include <cstring>
#include <hiredis/hiredis.h>
#include <iostream>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>

using namespace std;

Redis::Redis():_publishContext(nullptr), _subscribeContext(nullptr), _subCmdEventFd(-1)
{
    // 创建 eventfd 用于唤醒订阅线程处理新命令
    _subCmdEventFd = eventfd(0, EFD_NONBLOCK);
}

Redis::~Redis()
{
    if (_publishContext) {
        redisFree(_publishContext);
    }
    if (_subscribeContext) {
        redisFree(_subscribeContext);
    }
    if (_subCmdEventFd >= 0) {
        close(_subCmdEventFd);
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
    // hiredis context 非线程安全, 多线程并发 publish 需加锁
    lock_guard<mutex> lock(_publishMutex);
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
    // 只入队命令, 由订阅线程独占 _subscribeContext 发送, 避免并发操作 hiredis context
    {
        lock_guard<mutex> lock(_subCmdMutex);
        _subCommands.push("SUBSCRIBE " + to_string(channel));
    }
    // 唤醒订阅线程处理新命令
    if (_subCmdEventFd >= 0) {
        uint64_t one = 1;
        ssize_t ret = write(_subCmdEventFd, &one, sizeof(one));
        (void)ret;
    }
    return true;
}

// 向redis指定的通道 取消订阅消息
bool Redis::unsubscribe(int channel)
{
    {
        lock_guard<mutex> lock(_subCmdMutex);
        _subCommands.push("UNSUBSCRIBE " + to_string(channel));
    }
    if (_subCmdEventFd >= 0) {
        uint64_t one = 1;
        ssize_t ret = write(_subCmdEventFd, &one, sizeof(one));
        (void)ret;
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observeChannelMessage()
{
    redisReply* reply = nullptr;
    int redisFd = _subscribeContext->fd;
    int evFd = _subCmdEventFd;
    int maxFd = (redisFd > evFd ? redisFd : evFd) + 1;

    while (true) {
        // 1. 发送所有待发送的订阅命令(本线程独占 _subscribeContext 写)
        {
            lock_guard<mutex> lock(_subCmdMutex);
            while (!_subCommands.empty()) {
                const string cmd = _subCommands.front();
                _subCommands.pop();
                redisAppendCommand(_subscribeContext, cmd.c_str());
            }
            int done = 0;
            while (done == 0) {
                if (REDIS_ERR == redisBufferWrite(_subscribeContext, &done)) {
                    cerr << "failed to write subscribe command!" << endl;
                    break;
                }
            }
        }

        // 2. 先从读缓冲区解析已有响应(非阻塞)
        if (redisGetReplyFromReader(_subscribeContext, (void**)&reply) == REDIS_ERR) {
            break;
        }

        // 3. 读缓冲区无完整响应, select 等 socket 可读或新命令唤醒
        while (reply == nullptr) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(redisFd, &readfds);
            if (evFd >= 0) {
                FD_SET(evFd, &readfds);
            }

            // 100ms 超时兜底: 即使 eventfd 异常也能定期处理命令队列
            struct timeval tv = {0, 100000};
            int ret = select(maxFd, &readfds, nullptr, nullptr, &tv);
            if (ret < 0) {
                break;
            }
            if (evFd >= 0 && FD_ISSET(evFd, &readfds)) {
                // 有新命令入队, 清空 eventfd 并跳出, 回到开头处理命令
                uint64_t buf;
                while (read(evFd, &buf, sizeof(buf)) > 0) {
                }
                break;
            }
            if (FD_ISSET(redisFd, &readfds)) {
                if (redisBufferRead(_subscribeContext) == REDIS_ERR) {
                    break;
                }
            }
            if (redisGetReplyFromReader(_subscribeContext, (void**)&reply) == REDIS_ERR) {
                break;
            }
        }

        if (reply == nullptr) {
            continue; // 被新命令唤醒或超时, 回到循环开头
        }

        // 4. 处理响应: 只处理 "message" 三元组, 忽略 subscribe/unsubscribe 确认
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3 &&
            reply->element[0] != nullptr && reply->element[0]->str != nullptr &&
            strcmp(reply->element[0]->str, "message") == 0 &&
            reply->element[1] != nullptr && reply->element[1]->str != nullptr &&
            reply->element[2] != nullptr && reply->element[2]->str != nullptr) {
            // 给业务层上报通道上发生的消息
            _notifyMessageHandler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
        reply = nullptr;
    }
    cerr << "observeChannelMessage quit!" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::initNotifyHandler(std::function<void(int, std::string)> fn)
{
    _notifyMessageHandler = fn;
}
