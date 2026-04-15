#ifndef USER_H
#define USER_H

#include <string>

// 匹配user表的ORM DAO
class User
{
public:
    User(int id = -1, std::string name = "", std::string pwd = "", std::string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }

    void setId(int id) { this->id = id; }
    void setName(std::string name) { this->name = name; }
    void setPassword(std::string passwd) { this->password = passwd; }
    void setState(std::string state) { this->state = state; }

    int getId() const { return id; }
    std::string getName() const { return name; }
    std::string getPassword() const { return password; }
    std::string getState() const { return state; }

protected:
    int id;
    std::string name;
    std::string password;
    std::string state;
};

#endif // USER_H