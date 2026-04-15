#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"
#include <string>

// user表的数据操作类
class UserModel
{
public:
    // 增加方法
    // 为传入传出参数——id是数据库表自增字段
    bool insert(User& user);

    // 根据id查找用户
    User query(int id);

    // 更新用户状态state
    bool updateState(User& user);

    // 重置所有用户状态
    bool resetAllState();

};

#endif // USERMODEL_H