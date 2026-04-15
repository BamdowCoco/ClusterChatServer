#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>

// 维护群组信息和操作类
class GroupModel
{
public:
    // 创建群
    bool createGroup(Group& group);

    // 加入群
    bool joinGroup(int groupId, int userId, std::string role);

    // 查找群
    Group queryGroup(int groupId);

    // 查询用户所在群组信息
    std::vector<Group> queryGroupsByUserId(int userId);

    // 查询指定用户在指定群 其他用户id列表
    // 用于发送群聊消息
    std::vector<int> queryGroupUsers(int groupId, int userId);

    // 删除群
    bool removeGroup(int groupId);
};

#endif // GROUPMODEL_H