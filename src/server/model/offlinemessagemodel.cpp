#include "offlinemessagemodel.hpp"
#include "database.hpp"
#include <cstdio>
#include <mysql/mysql.h>

using namespace std;

// 插入操作
bool OfflineMessageModel::insert(const OfflineMessage& offlinemsg)
{
    // 1.构建sql语句
    char sql[1024] = {0};
    sprintf(sql, "INSERT INTO offlinemessage(user_id, message) VALUE (%d, '%s')",
            offlinemsg.getUserId(), offlinemsg.getMessage().c_str());
    
    // 2.创建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }
    
    return false;
}

// 删除用户的离线消息
bool OfflineMessageModel::remove(int userid)
{
    // 1.构建SQL语句
    char sql[1024] = {0};
    sprintf(sql, "DELETE FROM offlinemessage WHERE user_id = %d",
            userid);

    // 2.构建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }

    return false;
}

// 查询用户的离线消息
std::vector<std::string> OfflineMessageModel::query(int userid)
{
    // 1.组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "SELECT message FROM offlinemessage WHERE user_id = %d",
            userid);

    vector<string> vec;

    // 2.构建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            // 把对应用户的离线消息放入vec中
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                // 逐行读取
                vec.push_back(row[0]);
            }
            // 记得释放资源!!!
            mysql_free_result(res);
        }
    }
    return vec;
}
