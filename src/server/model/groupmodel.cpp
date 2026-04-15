#include "groupmodel.hpp"
#include "database.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mysql/mysql.h>

using namespace std;

// 创建群
bool GroupModel::createGroup(Group& group)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "INSERT INTO allgroup(group_name, group_desc) VALUE ('%s', '%s')",
            group.getGroupName().c_str(), group.getGroupDesc().c_str());

    // 2.定义MySQl对象
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            // 获取插入成功的用户 对应的 主键id
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

// 加入群
bool GroupModel::joinGroup(int groupId, int userId, std::string role)
{
    // 1.创建sql语句
    char sql[1024] = {0};
    sprintf(sql, "INSERT INTO groupuser(group_id, user_id, group_role) VALUE (%d, %d, '%s')",
            groupId, userId, role.c_str());

    // 2.构建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }

    return false;
}

// 查找群
Group GroupModel::queryGroup(int groupId)
{
    // 1.构建sql语句
    char sql[1024] = {0};
    sprintf(sql, "SELECT * FROM allgroup WHERE id = %d",
            groupId);

    Group group;
    // 2.构建MySQL对象
    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr) {
                group.setId(atoi(row[0]));
                group.setGroupName(row[1]);
                group.setGroupDesc(row[2]);
            }
            mysql_free_result(res);
        }
    }

    return group;
}

// 查询用户所在群组信息
std::vector<Group> GroupModel::queryGroupsByUserId(int userId)
{
    // 1.先查询群的基本信息
    vector<Group> groupVec;
    
    char sql[1024] = {0};
    sprintf(sql, "SELECT g.id, g.group_name, g.group_desc FROM allgroup g \
        INNER JOIN groupuser gu ON gu.group_id = g.id \
        WHERE gu.user_id = %d",
        userId);
        
        MySQL mysql;
        if (mysql.connect()) {
            MYSQL_RES* res = mysql.query(sql);
            if (res != nullptr) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) != nullptr) {
                    Group group;
                    group.setId(atoi(row[0]));
                    group.setGroupName(row[1]);
                    group.setGroupDesc(row[2]);
                    groupVec.push_back(group);
                }
                // 记得释放资源!!!
                mysql_free_result(res);
            }
        }

        // 2.再依次查询区内用户的信息
        for (Group& group : groupVec) {
            sprintf(sql, "SELECT u.id, u.name, u.state, gu.group_role FROM user u \
                        INNER JOIN groupuser gu ON gu.user_id = u.id \
                        WHERE gu.group_id = %d",
                    group.getId());

            MYSQL_RES* res = mysql.query(sql);
            if (res != nullptr) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) != nullptr) {
                    GroupUser user;
                    user.setId(atoi(row[0]));
                    user.setName(row[1]);
                    user.setState(row[2]);
                    user.setGroupRole(row[3]);
                    group.getUsers().push_back(user);
                }
                // 释放资源
                mysql_free_result(res);
            }
        }

        
    return groupVec;
}

// 查询指定用户在指定群 其他用户id列表
// 用于发送群聊消息
std::vector<int> GroupModel::queryGroupUsers(int groupId, int userId)
{
    vector<int> idVec;

    char sql[1024] = {0};
    sprintf(sql, "SELECT u.id FROM user u \
                INNER JOIN groupuser gu ON u.id = gu.user_id \
                WHERE gu.group_id = %d AND u.id != %d",
            groupId, userId);

    MySQL mysql;
    if (mysql.connect()) {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                idVec.push_back(atoi(row[0]));
            }
            // 记得释放资源!!!
            mysql_free_result(res);
        }
    }

    return idVec;
}

// 删除群
bool GroupModel::removeGroup(int groupId)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "DELETE FROM allgroup WHERE id = %d",
            groupId);

    // 2.定义MySQl对象
    MySQL mysql;
    if (mysql.connect()) {
        if (mysql.update(sql)) {
            return true;
        }
    }

    return false;
}