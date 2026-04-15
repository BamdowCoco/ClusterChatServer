#include "chatservice.hpp"
#include "offlinemessage.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <mutex>
#include <string>
#include <vector>

using namespace std;
using namespace muduo;

// 获取单例对象的接口函数
ChatService& ChatService::instance()
{
    static ChatService service;
    return service;
}

// 注册消息以及对应handler回调函数
ChatService::ChatService()
{
    _msgHandlerMap.insert(
        {LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert(
        {SIGNUP_MSG, std::bind(&ChatService::signup, this, _1, _2, _3)});
    _msgHandlerMap.insert(
        {ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert(
        {ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert(
        {CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert(
        {JOIN_GROUP_MSG, std::bind(&ChatService::joinGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert(
        {GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert(
        {LOGOUT_MSG, std::bind(&ChatService::logout, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect()) {
        // 设置redis上报业务层的回调函数
        _redis.initNotifyHandler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 获取消息对应的处理方法
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志: msgid无对应处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) {
        return [=](const TcpConnectionPtr& conn, json& js, Timestamp timestp) {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    return it->second;
}

// 处理登录业务 id password
void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    // LOG_INFO << "Processing login business!";
    int id = js["id"];
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPassword() == pwd) {
        if (user.getState() == "online") {
            // 该用户已登录, 不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "user already logged in";
            // response["errmsg"] = "该账号已登录";
            conn->send(response.dump());
        } else {
            // 登录成功, 记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({user.getId(), conn});
            }

            // redis 订阅 通道为用户id
            _redis.subscribe(user.getId());

            // 登录成功, 更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息
            vector<string> offlineMsgVec = _offlineMsgModel.query(user.getId());
            if (!offlineMsgVec.empty()) {
                // 若有, 装载到response
                response["offlinemsg"] = offlineMsgVec;
                // 删除数据库存储的离线消息
                _offlineMsgModel.remove(user.getId());
            }

            // (实际工程中 对于好友信息、群组信息 客户端也有相应的数据库和缓存存储)
            // 查询用户的好友信息
            vector<User> friendsVec = _friendModel.queryAllFriend(user.getId());
            if (!friendsVec.empty()) {
                /*
                // 由于vector<User>类型不能直接装载json
                // 故 User -> json -> string
                // 得到vector<string>类型
                */
                vector<string> friendsJsonVec;
                for (User& user : friendsVec) {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    friendsJsonVec.push_back(js.dump());
                }
                response["friends"] = friendsJsonVec;
            }

            // 查询用户群组信息
            vector<Group> groupsVec = _groupModel.queryGroupsByUserId(user.getId());
            if (!groupsVec.empty()) {
                vector<string> groupsJsonVec;
                for (Group& group : groupsVec) {
                    json js;
                    js["id"] = group.getId();
                    js["name"] = group.getGroupName();
                    js["desc"] = group.getGroupDesc();
                    vector<string> membersVec;
                    for (GroupUser& groupUser : group.getUsers()) {
                        json userJs;
                        userJs["id"] = groupUser.getId();
                        userJs["name"] = groupUser.getName();
                        userJs["state"] = groupUser.getState();
                        userJs["role"] = groupUser.getGroupRole();
                        membersVec.push_back(userJs.dump());
                    }
                    js["members"] = membersVec;
                    groupsJsonVec.push_back(js.dump());
                }
                response["groups"] = groupsJsonVec;
            }

            conn->send(response.dump());
        }
    } else {
        // 用户不存在/密码错误, 登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "invalid userid or password";
        // response["errmsg"] = "用户id或者密码错误";
        conn->send(response.dump());
    }
}

// 处理注册业务 name password
void ChatService::signup(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    // LOG_INFO << "Processing signup bussiness!";
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);

    bool state = _userModel.insert(user);
    if (state) {
        // 注册成功
        json response;
        response["msgid"] = SIGNUP_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    } else {
        // 注册失败
        json response;
        response["msgid"] = SIGNUP_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 处理一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    // LOG_INFO << "Processing one chat business!";
    // 判断接收者是否已登录——查找_userConnMap是否有对应连接
    int id = js["id"];
    int toid = js["to"];

    json response;
    response["msgid"] = ONE_CHAT_MSG_ACK;
    do {
        if (toid == id) {
            // 判断是否给自己发送聊天消息
            response["errno"] = 2;
            response["errmsg"] = "could not chat with yourself!";
            break;
        }
        // 访问连接表 要注意线程安全!!!
        {
            lock_guard<mutex> lock(_connMutex);
            auto it = _userConnMap.find(toid);
            if (it != _userConnMap.end()) {
                // 若有, 接收者在线, 则通过该连接转发消息
                it->second->send(js.dump());
                // 消息发送成功
                response["errno"] = 0;
                break;
            }
        }
        // 若无, 接收者可能在另一个服务器登录
        User user = _userModel.query(toid);
        if ("offline" == user.getState()) {
            // 接收者离线, 将消息存储到离线消息表中(数据库)
            OfflineMessage offlinemsg(toid, js.dump());
            bool ret = _offlineMsgModel.insert(offlinemsg);
            if (!ret) {
                // 插入离线消息失败 -> 发送失败
                response["errno"] = 1;
                response["errmsg"] = "failed to send message";
                break;
            }
        } else {
            // 接收者在线, 说明在另一台服务器, 将消息发布到消息队列(Redis)
            bool ret = _redis.publish(toid, js.dump());
            if (!ret) {
                // 插入离线消息失败 -> 发送失败
                response["errno"] = 1;
                response["errmsg"] = "failed to send message";
                break;
            }
        }
        // 消息发送成功
        response["errno"] = 0;
    } while(0);
    conn->send(response.dump());
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    User user;
    {
        // 注意互斥锁最小范围!!!
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++) {
            if (it->second == conn) {
                // 从map容器中删除相应的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 退出登录时，取消订阅
    _redis.unsubscribe(user.getId());

    // 更改用户状态 offline
    if (user.getId() != -1) {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 处理服务器关闭，重置user状态
void ChatService::reset()
{
    // 用户状态online -> offline
    _userModel.resetAllState();
}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    int userId = js["id"];
    int friendId = js["friendid"];

    json response;
    response["msgid"] = ADD_FRIEND_MSG_ACK;

    if (userId == friendId) {
        // 不能添加自己为好友
        response["errno"] = 1;
        response["errmsg"] = "cannot add self";
        // response["errmsg"] = "不能添加自己为好友";
        conn->send(response.dump());
        return;
    }

    Friend friendItem(userId, friendId);
    if (_friendModel.insert(friendItem)) {
        // 添加好友成功
        response["errno"] = 0;
    } else {
        // 添加好友失败
        // 不能重复添加好友 —— 数据库联合主键约束
        response["errno"] = 2;
        response["errmsg"] = "cannot add duplicate friend";
        // response["errmsg"] = "添加好友失败, 不能重复添加好友";
    }
    conn->send(response.dump());
}

// 创建群业务
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    int userId = js["id"];
    string name = js["name"];
    string desc = js["desc"];

    json response;
    response["msgid"] = CREATE_GROUP_MSG_ACK;

    Group group;
    group.setGroupName(name);
    group.setGroupDesc(desc);

    while (true) {
        if (_groupModel.createGroup(group)) {
            // 创建群聊成功
            if (_groupModel.joinGroup(group.getId(), userId, "creator")) {
                // 将创建用户加入到groupuser表 成功
                response["errno"] = 0;
                response["groupid"] = group.getId();
                break;
            } else {
                // 加入groupuser表 失败 撤销群聊创建
                _groupModel.removeGroup(group.getId());
            }
        }
        // 创建群聊失败
        response["errno"] = 1;
        response["errmsg"] = "failed to create group ";
        // response["errmsg"] = "创建群聊失败";
        break;
    }

    conn->send(response.dump());
}

// 加入群业务
void ChatService::joinGroup(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    int groupId = js["groupid"];
    int userId = js["userid"];
    string groupRole = js["role"];

    json response;
    response["msgid"] = JOIN_GROUP_MSG_ACK;

    while (true) {
        // 查找群是否存在
        // Group group = _groupModel.queryGroup(groupId);
        // if (group.getId() != groupId) {
        //     response["errno"] = 1;
        //     response["errmsg"] = "群聊不存在";
        //     break;
        // }

        // 查找用户是否存在
        // User user = _userModel.query(userId);
        // if (user.getId() != userId) {
        //     response["errno"] = 2;
        //     response["errmsg"] = "用户不存在";
        //     break;
        // }

        // GroupUser groupUser(groupId, userId, groupRole);
        if (_groupModel.joinGroup(groupId, userId, groupRole)) {
            // 加入群成功
            response["errno"] = 0;
        } else {
            // 加入群失败
            response["errno"] = 1;
            response["errmsg"] = "failed to join group";
            // response["errmsg"] = "加入群聊失败";
        }
        break;
    }
    conn->send(response.dump());
}

// 群聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    int userId = js["id"];
    // string from = js["from"];
    int groupId = js["groupid"];
    // string msg = js["msg"];

    json response;
    response["msgid"] = GROUP_CHAT_MSG_ACK;

    while (true) {
        // 查找群聊所有成员id
        vector<int> idVec = _groupModel.queryGroupUsers(groupId, userId);
        // 依次转发消息给群成员
        {
            // 访问临界区 记得上锁 确保线程安全
            lock_guard<mutex> lock(_connMutex);
            for (int id : idVec) {
                auto it = _userConnMap.find(id);
                if (it != _userConnMap.end()) {
                    // 对于存在连接 即在线成员 直接转发消息
                    it->second->send(js.dump());
                } else {
                    // 不存在连接 查询成员是否在线
                    User user = _userModel.query(id);
                    if ("online" == user.getState()) {
                        // 成员在线 发布到消息队列(Redis)
                        _redis.publish(id, js.dump());
                    } else {
                        // 成员离线 将消息插入到离线消息表
                        OfflineMessage offlineMsg(id, js.dump());
                        _offlineMsgModel.insert(offlineMsg);

                    }
                }
            }
        }
        response["errno"] = 0;
        break;
    }

    conn->send(response.dump());
}

// 处理注销业务
void ChatService::logout(const TcpConnectionPtr& conn, json& js, Timestamp timestp)
{
    int id = js["id"];
    {
        // 删除连接
        lock_guard<mutex> lock(_connMutex);
        _userConnMap.erase(id);
    }

    // 退出登录时，redis 取消订阅
    _redis.unsubscribe(id);

    User user;
    user.setId(id);
    user.setState("offline");

    json response;
    response["msgid"] = LOGOUT_MSG_ACK;

    if (_userModel.updateState(user)) {
        response["errno"] = 0;
    } else {
        response["errno"] = 1;
    }

    conn->send(response.dump());
}

// redis上报服务层回调函数
void ChatService::handleRedisSubscribeMessage(int userId, std::string message)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userId);
    if (it != _userConnMap.end()) {
        it->second->send(message);
        return;
    }

    // 找不到连接 离线 存储到离线
    OfflineMessage offlineMsg(userId, message);
    _offlineMsgModel.insert(offlineMsg);
}