#pragma once

#include <mysql/mysql.h>
#include <string>
#include <chrono>

/*
MySQL数据库操作模块
*/

class Connection
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    // 初始化数据库连接
    Connection();
    // 释放数据库连接资源
    ~Connection();

    // 连接数据库
    bool connect(std::string ip, unsigned short port,
                 std::string user, std::string password,
                 std::string dbname);

    // 更新操作 insert delete update 
    bool update(std::string sql);
    // 查询操作 select
    MYSQL_RES* query(std::string sql);

    // 刷新连接空闲时间点
    void refreshIdleStart();
    // 返回空闲存活时间 单位: s
    double getIdleAliveDuration() const;

    // 获取连接句柄
    MYSQL* getConn();


private:
    MYSQL *_conn;
    TimePoint _idleStart;
};