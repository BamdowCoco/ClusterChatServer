#include "friendmodel.hpp"
#include "database.hpp"
#include <cstdio>
#include <mysql/mysql.h>

using namespace std;

// 插入操作
bool FriendModel::insert(const Friend& friendItem)
{
    // 1.创建sql语句
    char sql[1024] = {0};
    sprintf(sql, "INSERT INTO friend(user_id, friend_id) VALUE (%d, %d)",
            friendItem.getUserId(), friendItem.getFriendId());

    // 2.构建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }

    return false;
}

// 返回用户的好友列表
std::vector<User> FriendModel::queryAllFriend(int userId)
{
    // 1.创建sql语句
    char sql[1024] = {0};
    sprintf(sql, "SELECT u.id, u.name, u.state FROM user u \
                INNER JOIN friend f ON u.id = f.friend_id \
                WHERE f.user_id = %d",
            userId);

    vector<User> vec;
    // 2.构建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            // 记得释放资源
            mysql_free_result(res);
        }
    }

    return vec;
}