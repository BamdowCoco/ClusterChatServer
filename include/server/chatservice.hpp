#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <functional>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TcpServer.h>
#include <unordered_map>
#include <mutex>

#include "redis.hpp"
#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
// #include "groupusermodel.hpp"

using json = nlohmann::json;

using muduo::Timestamp;
using muduo::net::TcpConnectionPtr;

// 表示处理消息回调的方法
using MsgHandler = std::function<void(const TcpConnectionPtr& conn, json& js, Timestamp timestp)>;

// 聊天服务器业务类
// 单例模式?
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService& instance();

    // 处理登录业务
    void login(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    // 处理注册业务
    void signup(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    // 处理一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    // 添加好友业务
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    
    // 创建群业务
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    // 加入群业务
    void joinGroup(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    // 群聊天业务
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    
    // 处理注销业务
    void logout(const TcpConnectionPtr& conn, json& js, Timestamp timestp);
    
    // 获取消息对应的处理方法
    MsgHandler getHandler(int msgid);

    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);

    // 处理服务器关闭，重置user状态
    void reset();

    // redis上报服务层回调函数
    void handleRedisSubscribeMessage(int userId, std::string message);
    
private:
    ChatService();

    // 存储消息id及其对应的处理方法
    std::unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接
    // 涉及多线程访问修改 需要添加线程互斥操作!
    std::unordered_map<int, TcpConnectionPtr> _userConnMap;

    // 定义互斥锁 保证_userConnMap线程安全
    std::mutex _connMutex;

    // 数据操作类对象
    UserModel _userModel;
    OfflineMessageModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;
    // GroupUserModel _groupUserModel;

    // redis 操作对象
    Redis _redis;
};

#endif // CHATSERVICE_H