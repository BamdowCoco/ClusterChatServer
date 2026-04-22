#include "session.hpp"
#include <semaphore.h>

UserSession::UserSession()
{
    sem_init(&m_rwSem, 0, 0);
}

UserSession::~UserSession()
{
    sem_destroy(&m_rwSem);
}

UserSession& UserSession::instance()
{
    static UserSession userSession;
    return userSession;
}


/* set 方法 */
void UserSession::setCurrentUser(User&& user)
{
    m_currentUser = std::move(user);
}

void UserSession::setFriendList(std::vector<User>&& firendList)
{
    m_friendList = std::move(firendList);
}

void UserSession::setGroupList(std::vector<Group>&& groupList)
{
    m_groupList = std::move(groupList);
}

void UserSession::setMainMenuRunning(bool running)
{
    m_isMainMenuRunning = running;
}


/* get 方法 */
const User& UserSession::getCurrentUser() const
{
    return m_currentUser;
}

const std::vector<User>& UserSession::getFriendList() const
{
    return m_friendList;
}

const std::vector<Group>& UserSession::getGroupList() const
{
    return m_groupList;
}

bool UserSession::isMainMenuRunning()
{
    return m_isMainMenuRunning;
}


// 清空会话
void UserSession::clear()
{
    m_currentUser = User();
    m_friendList.clear();
    m_groupList.clear();
    m_isMainMenuRunning = false;
    m_isLoginSuccess.store(false);
}

/* 业务相关 */
// 添加好友
void UserSession::addFriend(User&& user)
{
    m_friendList.emplace_back(user);
}

// 添加群聊
void UserSession::addGroup(Group&& group)
{
    m_groupList.emplace_back(group);
}

/* 线程通信相关 */
// 信号量P操作
void UserSession::rwWait()
{
    sem_wait(&m_rwSem);
}
// 信号量V操作
void UserSession::rwPost()
{
    sem_post(&m_rwSem);
}

// 登录状态
void UserSession::setLoginSuccess(bool success)
{
    m_isLoginSuccess.store(success);
}
bool UserSession::isLoginSuccess()
{
    return m_isLoginSuccess.load();
}