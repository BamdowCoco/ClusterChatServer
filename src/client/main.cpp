#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>

#include "groupuser.hpp"
#include "json.hpp"
using json = nlohmann::json;
using namespace std;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "user.hpp"
#include "group.hpp"
#include "public.hpp"

#ifdef DEBUGS
// 调试模式: 执行代码块
    #define DEBUG_SCOPE(code) \
        do {                  \
            code              \
        } while (0)

    #define DEBUG_LOG(msg) std::cout << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " " << (msg) << std::endl
#else
    #define DEBUG_SCOPE(code) ((void)0)
    #define DEBUG_LOG(msg) ((void)0)
#endif

// 记录当前登录用户信息
User g_currentUser;
// 记录当前登录用户好友信息
vector<User> g_currentUserFriendVec;
// 记录当前登录用户群组信息
vector<Group> g_currentUserGroupVec;
// 显示当前登录用户所有信息
void showCurrentUserInfo();

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间(聊天信息需要添加时间信息)
std::string getCurrentTime();
// 安全输入整数
int getIntInput();
// 主聊天页面程序(登录后)
void mainMenu(int clientfd);
// 控制聊天页面程序
bool isMainMenuRunning;


// 聊天客户端程序实现 main线程作为发送线程 子线程作为接收线程
int main(int argc, char* argv[])
{
    /*
    if (argc != 3) {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(1);
    }

    // 解析命令行参数的ip和端口
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);
    */
    char ip[] = "127.0.0.1";
    uint16_t port = 8000;

    // 创建socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        cerr << "socket create error" << endl;
        exit(2);
    }

    // 填写client连接server IP+port
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    // 请求连接服务端
    if (connect(clientfd, (sockaddr*)&server, sizeof(server))) {
        cerr << "connect server error" << endl;
        exit(3);
    }

    // main线程: 接收用户输入 发送数据
    while (true) {
        // 显示首页面菜单: 登录、注册、退出
        cout << "=========================" << endl;
        cout << "1. login" << endl;
        cout << "2. sign up" << endl;
        cout << "3. quit" << endl;
        cout << "=========================" << endl;
        cout << "choice:";
        int choice = 0;
        choice = getIntInput();

        // DEBUG_LOG(choice);

        switch (choice) {
        case 1: { // login
            int id;
            char pwd[50];
            cout << "userid:";
            id = getIntInput();
            cout << "password:";
            cin.getline(pwd, 50);

            // 组装json
            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            // 发送请求
            int len = send(clientfd, request.c_str(), request.size()+1, 0);
            if (len <= 0) {
                cerr << "send login request error:" << request << endl;
            } else {
                // 接收响应
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, sizeof(buffer), 0);
                if (len <= 0) {
                    cerr << "receive login response error" << endl;
                } else {
                    // 响应反序列化
                    json response = json::parse(buffer);

                    // !!!根据错误代码判断请求是否成功
                    if (response["errno"] != 0) {
                        cerr << "failed to log in: " << response["errmsg"] << endl;
                    } else {
                        // 保存登录用户信息
                        g_currentUser.setId(response["id"]);
                        g_currentUser.setName(response["name"]);

                        // 保存登录用户好友
                        if (response.contains("friends")) {
                            vector<string> friendJsonVec = response["friends"];
                            for (string& str : friendJsonVec) {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"]);
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendVec.push_back(user);
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
        
                                g_currentUserGroupVec.push_back(group);
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

                        // 登录成功, 创建子线程接收数据 只创建一次
                        static int readThreadNum = 0;
                        if (readThreadNum == 0) {
                            std::thread readTask(readTaskHandler, clientfd); // pthread_create
                            readTask.detach(); // pthread_detach
                            readThreadNum++;
                        }
    
                        // 登录成功进入聊天页面
                        mainMenu(clientfd);
                    }
                }
            }

            break;
        }
        case 2: { // signup
            char name[50];
            char pwd[50];
            cout << "username:";
            cin.getline(name, 50);
            cout << "password:";
            cin.getline(pwd, 50);

            // 组装json
            json js;
            js["msgid"] = SIGNUP_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            // 发送数据至服务端
            int len = send(clientfd, request.c_str(), request.size()+1, 0);
            if (len <= 0) {
                cerr << "send signup msg error:" << request << endl;
            } else {
                // 接收服务端的响应数据
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, sizeof(buffer), 0);
                if (len == -1) {
                    cerr << "recv signup response error" << endl;
                } else {
                    // 反序列化为json
                    json response = json::parse(buffer);
                    if (response["errno"].get<int>() != 0) {
                        // 注册失败
                        cerr << name << " already exists, signup fails..." << endl;
                    } else {
                        // 注册成功
                        cout << name << " sign up success, userid is " << response["id"]
                             << ", do not forget it!" << endl;
                        
                    }
                }
            }

            break;
        }
        case 3: { // quit
            close(clientfd);
            exit(0);
            break;
        }
        default: { // 意外输入
            cerr << "invalid input" << endl;
            break;
        }
        }
    }
}

// 显示当前登录用户所有信息
void showCurrentUserInfo()
{
    cout << "======================login user======================" << endl;
    cout << "id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "---------------------Friend List---------------------" << endl;
    for (User& user : g_currentUserFriendVec) {
        cout << user.getId() << '\t'
             << user.getName() << '\t'
             << user.getState() << endl;
    }
    cout << "---------------------Group List---------------------" << endl;
    for (Group& group : g_currentUserGroupVec) {
        cout << group.getId() << '\t'
             << group.getGroupName() << '\t'
             << group.getGroupDesc() << endl;
        cout << endl;
        for (GroupUser& user : group.getUsers()) {
            cout << user.getId() << '\t'
                 << user.getName() << '\t'
                 << user.getState() << '\t'
                 << user.getGroupRole() << endl;
        }
        cout << "----------------------------------------------" << endl;
    }
    cout << "======================================================" << endl;
}

// 接收线程
void readTaskHandler(int clientfd)
{
    while (true) {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, sizeof(buffer), 0);
        DEBUG_LOG(buffer);
        if (len == -1 || len == 0) {
            // close(clientfd);
            DEBUG_LOG("close");
            exit(0);
        }

        // 接收服务端数据 反序列化为json
        json js = json::parse(buffer);
        switch (js["msgid"].get<int>()) {
        case ONE_CHAT_MSG: {
            cout << js["time"].get<string>() << " [" << js["id"] << "]"
                 << js["from"].get<string>() << " said: " << js["msg"].get<string>() << endl;
            break;
        }
        case GROUP_CHAT_MSG: {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>()
                 << " [" << js["id"] << "]" << js["from"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            break;
        }
        default: {
            break;
        }
        }
    }
}

// 获取系统时间(聊天信息需要添加时间信息)
std::string getCurrentTime()
{
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    TimePoint now = Clock::now();
    std::time_t in_time_t = Clock::to_time_t(now);

    std::tm bt;
    localtime_r(&in_time_t, &bt);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &bt);

    return std::string(buffer);
}

// 安全输入整数
int getIntInput()
{
    int value;
    while (true) {
        string input;
        getline(cin, input); // 读取整行
        stringstream ss(input); // 创建字符串流

        // 尝试从字符串流提取正数到value
        // 并判断是否读到流末尾 ss.eof()
        if (ss >> value && ss.eof()) {
            return value;
        }
        cout << "无效输入, 请输入整数: ";
    }
}

// "help" command handler
void help(int = 0, string = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "joingroup" command handler
void joingroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "logout" command handler
void logout(int, string);

// 系统支持的客户端 命令-功能&格式 映射表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"joingroup", "加入群组, 格式joingroup:groupid"},
    {"groupchat", "群聊, 格式groupchat:groupid:message"},
    {"logout", "退出当前帐号, 格式logout"}};

// 注册系统支持客户端命令处理方法
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"joingroup", joingroup},
    {"groupchat", groupchat},
    {"logout", logout}};

// 主聊天页面程序(登录后)
void mainMenu(int clientfd)
{
    help();
    isMainMenuRunning = true;
    char buffer[1024] = {0};
    while (isMainMenuRunning) {
        // 读取输入命令字符串
        cin.getline(buffer, sizeof(buffer));
        string commandbuf(buffer);
        // 截取命令
        string command;
        int index = commandbuf.find(':');
        if (index == -1) {
            command = commandbuf;
        } else {
            command = commandbuf.substr(0, index);
        }
        // 查找命令处理函数
        auto it = commandHandlerMap.find(command);
        if (it != commandHandlerMap.end()) {
            // 存在命令
            it->second(clientfd, commandbuf.substr(index + 1));
            if (g_currentUser.getId() == -1) {
                // 已成功注销
                break;
            }
        } else {
            // 不存在命令
            DEBUG_LOG(command);
            cerr << "invalid command!" << endl;
        }
    }
}

// "help" command handler
void help(int, string)
{
    cout << "command list >>>" << endl;
    for (auto& p : commandMap) {
        cout << p.first << " :" << p.second << endl;
    }
    cout << ">>>>>>>>>>>>>>>>" << endl;
}
// "chat" command handler
void chat(int clientfd, string str)
{
    int index = str.find(':');
    if (index == -1) {
        cerr << "chat command invalid!" << endl;
        return;
    }
    int to = atoi(str.substr(0, index).c_str());
    if (to == g_currentUser.getId()) {
        cerr << "could not chat with yourself!" << endl;
        return;
    }

    string msg = str.substr(index + 1);
    if (msg.empty()) {
        cerr << "message should not be empty!" << endl;
        return;
    }

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["from"] = g_currentUser.getName();
    js["to"] = to;
    js["msg"] = msg;
    js["time"] = getCurrentTime();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), buffer.size()+1, 0);
    if(len <= 0) {
        cerr << "failed to send chat msg: " << buffer << endl;
    }
}
// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());

    json request;
    request["msgid"] = ADD_FRIEND_MSG;
    request["id"] = g_currentUser.getId();
    request["friendid"] = friendid;
    string buffer = request.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (len <= 0) {
        cerr << "failed to send add friend msg: " << buffer << endl;
    }
}
// "creategroup" command handler
void creategroup(int clientfd, string str)
{
    int index = str.find(':');
    if (index == -1) {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
    string name = str.substr(0, index);
    string desc = str.substr(index + 1);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = name;
    js["desc"] = desc;

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), buffer.size()+1, 0);
    if(len <= 0) {
        cerr << "failed to send creategroup msg: " << buffer << endl;
    }
}
// "joingroup" command handler
void joingroup(int clientfd, string str)
{
    int groupId = atoi(str.c_str());
    if (groupId < 0) {
        DEBUG_SCOPE(
            cerr << "\033[31mError:\033[0m [joingroup] Invalid groupId (must be > 0)." << endl;
            cerr << "\033[33mUsage:\033[0m joingroup:<positive_integer>  e.g., joingroup:12345" << endl;);
        return;
    }

    json js;
    js["msgid"] = JOIN_GROUP_MSG;
    js["groupid"] = groupId;
    js["userid"] = g_currentUser.getId();
    js["role"] = "normal";

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (len <= 0) {
        cerr << "failed to send joingroup msg: " << buffer << endl;
    }
}
// "groupchat" command handler
void groupchat(int clientfd, string str)
{
    int index = str.find(':');
    if (index == -1) {
        cerr << "groupchat command invalid!" << endl;
        return;
    }
    int groupId = atoi(str.substr(0, index).c_str());
    string msg = str.substr(index + 1);
    
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["from"] = g_currentUser.getName();
    js["groupid"] = groupId;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    
    string buffer = js.dump();
    // DEBUG_LOG(buffer);
    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (len <= 0) {
        cerr << "failed to send groupchat msg: " << buffer << endl;
    }
}
// "logout" command handler
void logout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (len <= 0) {
        cerr << "failed to send logout msg: " << buffer << endl;
    } else {
        isMainMenuRunning = false;
        // 清空缓存
        g_currentUserFriendVec.clear();
        g_currentUserGroupVec.clear();
        g_currentUser = User();
    }
}
