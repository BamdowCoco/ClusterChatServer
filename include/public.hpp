#ifndef PUBLIC_H
#define PUBLIC_H

/*
server和client公共文件
*/

// 消息代码
enum EnMsgType
{
    LOGIN_MSG = 1, // 登录消息
    LOGIN_MSG_ACK, // 登录响应消息
    SIGNUP_MSG, // 注册消息
    SIGNUP_MSG_ACK, // 注册响应消息
    ONE_CHAT_MSG, // 一对一聊天消息
    ONE_CHAT_MSG_ACK, // 一对一聊天响应消息
    ADD_FRIEND_MSG, // 添加好友消息
    ADD_FRIEND_MSG_ACK, // 添加好友响应消息
    CREATE_GROUP_MSG, // 创建群消息
    CREATE_GROUP_MSG_ACK, // 创建群响应消息
    JOIN_GROUP_MSG, // 加入群消息
    JOIN_GROUP_MSG_ACK, // 加入群响应消息
    GROUP_CHAT_MSG, // 群聊天消息
    GROUP_CHAT_MSG_ACK, // 群聊天响应消息
    LOGOUT_MSG, // 注销消息
    LOGOUT_MSG_ACK, // 注销响应消息

};

#endif  // PUBLIC_H