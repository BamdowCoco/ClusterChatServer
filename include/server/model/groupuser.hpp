#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"
#include <string>

// 群组成员
class GroupUser : public User
{
public:
    void setGroupRole(std::string group_role) { this->group_role = group_role; }
    std::string getGroupRole() const { return group_role; }

private:
    std::string group_role; // ENUM('creator', 'normal')
};

#endif // GROUPUSER_H