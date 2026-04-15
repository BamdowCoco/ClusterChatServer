#include "usermodel.hpp"
#include "database.hpp"
#include <cstdio>
#include <cstdlib>
#include <mysql/mysql.h>

using namespace std;

// user表增加方法
bool UserModel::insert(User& user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "INSERT INTO user(name, password, state) VALUES('%s', '%s', '%s')",
            user.getName().c_str(), user.getPassword().c_str(), user.getState().c_str());

    // 2.定义MySQl对象
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            // 获取插入成功的用户 对应的 主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

// 根据id查找用户
User UserModel::query(int id)
{
    // 1.组织sql语句
    char sql[1024] = {0};
    sprintf(sql, "SELECT * FROM user WHERE id = %d",
            id);

    // 2.创建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        // 执行查询语句
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            // 获取查询到的行
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr) {
                User user;
                // 通过[] 得到各个表项
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPassword(row[2]);
                user.setState(row[3]);

                // 释放资源
                mysql_free_result(res);
                return user;
            }
            // 释放资源
            mysql_free_result(res);
        }
    }

    return User();
}

// 更新用户状态state
bool UserModel::updateState(User& user)
{
    // 1.构造sql语句
    char sql[1024] = {0};
    sprintf(sql, "UPDATE user SET state = '%s' WHERE id = %d",
            user.getState().c_str(), user.getId());

    // 2.构造MySQL对象执行语句
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }

    return false;
}

// 更新方法
// 重置所有用户状态
bool UserModel::resetAllState()
{
    // 1.构造sql语句
    char sql[1024] = {0};
    sprintf(sql, "UPDATE user SET state = 'offline' WHERE state = 'online'");

    // 2.构造MySQL对象执行语句
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }

    return false;
}