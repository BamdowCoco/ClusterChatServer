#ifndef SESSION_H
#define SESSION_H

#include <vector>
#include <semaphore.h>
#include <atomic>

#include "user.hpp"
#include "group.hpp"

class UserSession
{
public:
    static UserSession& instance();

    /* set 方法 */
    void setCurrentUser(User&& user);
    void setFriendList(std::vector<User>&& firendList);
    void setGroupList(std::vector<Group>&& groupList);
    void setMainMenuRunning(bool running);
    
    /* get 方法 */
    const User& getCurrentUser () const;
    const std::vector<User>& getFriendList() const;
    const std::vector<Group>& getGroupList() const;
    bool isMainMenuRunning();
    
    // 清空会话
    void clear();
    
    /* 业务相关 */
    // 添加好友
    void addFriend(User&& user);
    // 添加群聊
    void addGroup(Group&& group);

    /* 线程通信相关 */
    // 信号量P操作
    void rwWait();
    // 信号量V操作
    void rwPost();
    // 登录状态
    void setLoginSuccess(bool success);
    bool isLoginSuccess();

private:
    UserSession();

    ~UserSession();

    // 记录当前登录用户信息
    User m_currentUser;
    // 记录当前登录用户好友信息
    std::vector<User> m_friendList;
    // 记录当前登录用户群组信息
    std::vector<Group> m_groupList;

    // 控制聊天页面程序
    bool m_isMainMenuRunning = false;

    /* 用于读写线程之间的通信 */
    sem_t m_rwSem;
    // 记录登录状态
    std::atomic<bool> m_isLoginSuccess{false};
    // 记录注册状态
};

#endif // SESSION_H