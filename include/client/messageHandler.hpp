#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include "json.hpp"
using json = nlohmann::json;

class MessageHandler
{
public:
    // 处理登录响应
    static void handleLoginResponse(const json& response);
    // 显示当前登录用户所有信息
    static void showCurrentUserInfo();
    // 处理注册响应
    static void handleSignupResponse(const json& response);
};

#endif // MESSAGE_HANDLER_H