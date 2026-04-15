#ifndef GROUP_H
#define GROUP_H

#include "groupuser.hpp"
#include <string>
#include <vector>

// allgroup表ORM
class Group
{
public:
    Group(int id = -1, std::string group_name = "", std::string group_desc = "")
    {
        this->id = id;
        this->group_name = group_name;
        this->group_desc = group_desc;
    }

    void setId(int id) { this->id = id; }
    void setGroupName(std::string group_name) { this->group_name = group_name; }
    void setGroupDesc(std::string group_desc) { this->group_desc = group_desc; }

    int getId() const { return id; }
    std::string getGroupName() const { return group_name; }
    std::string getGroupDesc() const { return group_desc; }
    std::vector<GroupUser>& getUsers() { return this->users; }

private:
    int id;
    std::string group_name;
    std::string group_desc;
    std::vector<GroupUser> users;
};

#endif // GROUP_H