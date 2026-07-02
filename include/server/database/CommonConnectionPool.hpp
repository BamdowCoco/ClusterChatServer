#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>

#include "Connection.hpp"

/*
连接池功能模块
*/

class ConnectionPool
{
public:
    // 禁止拷贝和移动
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    // 获取连接池单例
    static ConnectionPool& getInstance();

    // 外部接口 获取可用空闲连接
    // 返回智能指针 方便自动回收连接资源
    std::shared_ptr<Connection> getConnection();

    // 关闭连接池
    // 结束生产者线程和定时回收线程
    void stop();

    ~ConnectionPool();

private:
    // 单例模式 构造私有化
    ConnectionPool();

    // 从配置文件中加载配置项
    bool loadConfigFile();

    // 运行在子线程 生产新连接
    void produceConnectionTask();
    
    // 运行在子线程 定时扫描超过_maxIdleTime的空闲连接 并回收相应资源
    void scanConnectionTask();

    // 连接数据库相关
    std::string _ip;
    unsigned short _port;
    std::string _username;
    std::string _password;
    std::string _dbname;

    int _initSize; // 初始连接量
    int _maxSize; // 最大连接量
    int _maxIdleTime; // 连接最大空闲时间 单位: s
    int _connectionTimeout; // 获取连接超时时间 单位: ms

    // 连接队列
    std::queue<Connection*> _connectionQue; // 存储连接队列
    std::mutex _queueMutex; // 维护连接队列线程安全互斥锁
    std::atomic<int> _connectionCount; // 创建连接数

    // 条件变量 用于生产线程和连接消费线程的通信
    std::condition_variable _cvProducer; // 连接生产者 条件变量
    std::condition_variable _cvConsumer; // 连接消费者 条件变量

    // 停止标志
    bool _isStop;
};