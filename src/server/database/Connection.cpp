#include "Connection.hpp"
#include "logger.hpp"

#include <chrono>
#include <mysql/mysql.h>

// 初始化数据库连接
Connection::Connection()
{
    _conn = mysql_init(nullptr);
}
// 释放数据库连接资源
Connection::~Connection()
{
    if(_conn != nullptr) {
        mysql_close(_conn);
    }
}

// 连接数据库
bool Connection::connect(std::string ip, unsigned short port,
                         std::string user, std::string password,
                         std::string dbname)
{
    MYSQL* p = mysql_real_connect(_conn, ip.c_str(), user.c_str(), password.c_str(),
                                  dbname.c_str(), port, nullptr, 0);
    return p != nullptr;
}

// 更新操作 insert delete update
bool Connection::update(std::string sql)
{
    if(mysql_query(_conn, sql.c_str())) {
        // LOG("更新失败: " + sql);
        return false;
    }
    return true;
}
// 查询操作 select
MYSQL_RES* Connection::query(std::string sql)
{
    if(mysql_query(_conn, sql.c_str())) {
        LOG("查询失败: " + sql);
        return nullptr;
    }
    return mysql_use_result(_conn);
}

// 刷新连接空闲时间点
void Connection::refreshIdleStart() {
    _idleStart = Clock::now();
}

// 返回空闲存活时间 单位: s
double Connection::getIdleAliveDuration() const{
    return std::chrono::duration_cast<Duration>(Clock::now()-_idleStart).count();
}

// 获取连接句柄
MYSQL* Connection::getConn()
{
    return _conn;
}