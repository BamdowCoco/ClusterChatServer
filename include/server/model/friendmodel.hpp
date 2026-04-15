#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H
#include "friend.hpp"
#include "user.hpp"
#include <vector>

// friend表 操作类
class FriendModel
{
public:
    // 插入操作
    bool insert(const Friend& friendItem);

    // 返回用户的好友列表
    std::vector<User> queryAllFriend(int userId);
};

#endif // FRIENDMODEL_H
