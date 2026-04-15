#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include "offlinemessage.hpp"
#include <vector>
#include <string>

// 离线信息表操作类
class OfflineMessageModel
{
public:
    // 插入新的离线消息
    bool insert(const OfflineMessage& offlinemsg);

    // 删除用户的离线消息
    bool remove(int userid);

    // 查询用户的离线消息
    std::vector<std::string> query(int userid);

};

#endif // OFFLINEMESSAGEMODEL_H