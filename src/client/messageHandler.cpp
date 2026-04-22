#include "messageHandler.hpp"
#include <iostream>
using namespace std;

#include <string>

#include "public.hpp"
#include "session.hpp"

// 处理登录响应
void MessageHandler::handleLoginResponse(const json& response)
{
    auto& session = UserSession::instance();
    // !!!根据错误代码判断请求是否成功
    if (response["errno"] != 0) {
        // 登录失败
        session.setLoginSuccess(false);
        // g_isLoginSuccess.store(false);
        cerr << "failed to log in: " << response["errmsg"] << endl;
    } else {
        // 登录成功
        session.setLoginSuccess(true);
        // g_isLoginSuccess.store(true);
        // 保存登录用户信息
        User user(response["id"], response["name"], "", "online");
        session.setCurrentUser(std::move(user));
        // g_currentUser.setId(response["id"]);
        // g_currentUser.setName(response["name"]);

        // 保存登录用户好友
        if (response.contains("friends")) {
            vector<string> friendJsonVec = response["friends"];
            for (string& str : friendJsonVec) {
                json js = json::parse(str);
                User user;
                user.setId(js["id"]);
                user.setName(js["name"]);
                user.setState(js["state"]);
                session.addFriend(std::move(user));
                // g_currentUserFriendVec.push_back(user);
            }
        }

        // 保存登录用户群组信息
        if (response.contains("groups")) {
            vector<string> groupsJsonVec = response["groups"];
            for (string& groupStr : groupsJsonVec) {
                json groupJs = json::parse(groupStr);
                Group group;
                group.setId(groupJs["id"]);
                group.setGroupName(groupJs["name"]);
                group.setGroupDesc(groupJs["desc"]);

                // 反序列化群员数组
                vector<string> membersJsonVec = groupJs["members"];
                for (string& userStr : membersJsonVec) {
                    json userJs = json::parse(userStr);
                    GroupUser user;
                    user.setId(userJs["id"]);
                    user.setName(userJs["name"]);
                    user.setState(userJs["state"]);
                    user.setGroupRole(userJs["role"]);
                    group.getUsers().push_back(user);
                }

                session.addGroup(std::move(group));
                // g_currentUserGroupVec.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserInfo();

        // 处理离线消息 显示好友消息和群组消息
        if (response.contains("offlinemsg")) {
            vector<string> offlineMsgVec = response["offlinemsg"];
            for (string& str : offlineMsgVec) {
                json msgJs = json::parse(str);
                if (msgJs["msgid"] == ONE_CHAT_MSG) {
                    // 私聊
                    cout << msgJs["time"].get<string>() << " [" << msgJs["id"] << "]"
                         << msgJs["from"].get<string>() << " said: " << msgJs["msg"].get<string>() << endl;
                } else if (msgJs["msgid"] == GROUP_CHAT_MSG) {
                    // 群聊
                    cout << "群消息[" << msgJs["groupid"] << "]:" << msgJs["time"].get<string>()
                         << " [" << msgJs["id"] << "]" << msgJs["from"].get<string>()
                         << " said: " << msgJs["msg"].get<string>() << endl;
                    // cout << msgJs["time"].get<string>() << " group[" << msgJs["groupid"] << "]"
                    //      << " [" << msgJs["id"] << "]" << msgJs["from"].get<string>()
                    //      << " said: " << msgJs["msg"].get<string>() << endl;
                }
            }
        }
    }
}

// 显示当前登录用户所有信息
void MessageHandler::showCurrentUserInfo()
{
    auto& session = UserSession::instance();
    cout << "======================login user======================" << endl;
    cout << "id:" << session.getCurrentUser().getId() << " name:" << session.getCurrentUser().getName() << endl;
    // cout << "id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "---------------------Friend List---------------------" << endl;
    if (session.getFriendList().empty()) {
        cout << "None..." << endl;
    } else {
        for (const User& user : session.getFriendList()) {
            // for (User& user : g_currentUserFriendVec) {
            cout << user.getId() << '\t'
                 << user.getName() << '\t'
                 << user.getState() << endl;
        }
    }
    cout << "---------------------Group List---------------------" << endl;
    if (session.getGroupList().empty()) {
        cout << "None..." << endl;
    } else {
        for (const Group& group : session.getGroupList()) {
            // for (Group& group : g_currentUserGroupVec) {
            cout << group.getId() << '\t'
                 << group.getGroupName() << '\t'
                 << group.getGroupDesc() << endl;
            cout << endl;
            for (const GroupUser& user : group.getUsers()) {
                cout << user.getId() << '\t'
                     << user.getName() << '\t'
                     << user.getState() << '\t'
                     << user.getGroupRole() << endl;
            }
            cout << "----------------------------------------------" << endl;
        }
    }
    cout << "======================================================" << endl;
}

// 处理注册响应
void MessageHandler::handleSignupResponse(const json& response)
{
    if (response["errno"].get<int>() != 0) {
        // 注册失败
        cerr << "user:" << response["name"].get<string>() << " already exists, signup fails..." << endl;
    } else {
        // 注册成功
        cout << "user:" << response["name"].get<string>() << " sign up success, userid is " << response["id"]
             << ", do not forget it!" << endl;
    }
}