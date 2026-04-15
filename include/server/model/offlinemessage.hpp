#ifndef OFFLINEMESSAGE_H
#define OFFLINEMESSAGE_H

#include <string>

// offlinemessage表 ORM
class OfflineMessage
{
public:
    OfflineMessage(int user_id = -1, std::string message = "")
    {
        this->user_id = user_id;
        this->message = message;
    }

    void setUserId(int user_id) { this->user_id = user_id; }
    void setMessage(std::string message) { this->message = message; }

    int getUserId() const { return user_id; }
    const std::string getMessage() const { return message; }

private:
    int user_id;
    std::string message;
};

#endif // OFFLINEMESSAGE_H