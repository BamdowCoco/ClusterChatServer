#ifndef FRIEND_H
#define FRIEND_H

// friend表 ORM
class Friend
{
public:
    Friend(int user_id = -1, int friend_id = -1)
    {
        this->user_id = user_id;
        this->friend_id = friend_id;
    }
    
    void setUserId(int user_id) { this->user_id = user_id; }
    void setFriendId(int user_id) { this->user_id = user_id; }

    int getUserId() const { return user_id; }
    int getFriendId() const { return friend_id; }

private:
    int user_id;
    int friend_id;
};
#endif // FRIEND_H