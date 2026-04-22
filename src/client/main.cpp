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
#include <semaphore.h>
#include <atomic>

#include "user.hpp"
#include "group.hpp"
#include "public.hpp"

#include "session.hpp"
#include "receiverThread.hpp"

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

// 获取系统时间(聊天信息需要添加时间信息)
std::string getCurrentTime();
// 安全输入整数
int getIntInput();
// 主聊天页面程序(登录后)
void mainMenu(int clientfd);

// 聊天客户端程序实现 main线程作为发送线程 子线程作为接收线程
int main(int argc, char* argv[])
{
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

    // 会话初始化
    auto& session = UserSession::instance();

    // 初始化读写线程信号量(已在UserSession中初始化)

    // 连接服务器成功, 创建子线程接收数据 只创建一次
    std::thread readTask(readTaskHandler, clientfd); // pthread_create
    readTask.detach(); // pthread_detach

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
            session.setLoginSuccess(false);

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
                // 等待子线程接收响应 返回登录结果
                session.rwWait();
                // sem_wait(&rwSem);
                if (session.isLoginSuccess()) {
                // if (g_isLoginSuccess.load()) {
                    // 登录成功 进入聊天主界面
                    session.setMainMenuRunning(true);
                    // isMainMenuRunning = true;
                    mainMenu(clientfd);
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
                // 等待接收子线程处理响应
                session.rwWait();
            }

            break;
        }
        case 3: { // quit
            close(clientfd);
            // 信号量已在 UserSession 析构函数释放
            exit(0);
        }
        default: { // 意外输入
            cerr << "invalid input" << endl;
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
    auto& session = UserSession::instance();
    help();
    char buffer[1024] = {0};
    while (session.isMainMenuRunning()) {
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
            if (session.getCurrentUser().getId() == -1) {
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
    auto& session = UserSession::instance();
    int index = str.find(':');
    if (index == -1) {
        cerr << "chat command invalid!" << endl;
        return;
    }
    int to = atoi(str.substr(0, index).c_str());
    if (to == session.getCurrentUser().getId()) {
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
    js["id"] = session.getCurrentUser().getId();
    js["from"] = session.getCurrentUser().getName();
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
    auto& session = UserSession::instance();
    int friendid = atoi(str.c_str());

    json request;
    request["msgid"] = ADD_FRIEND_MSG;
    request["id"] = session.getCurrentUser().getId();
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
    auto& session = UserSession::instance();
    int index = str.find(':');
    if (index == -1) {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
    string name = str.substr(0, index);
    string desc = str.substr(index + 1);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = session.getCurrentUser().getId();
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
    auto& session = UserSession::instance();
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
    js["userid"] = session.getCurrentUser().getId();
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
    auto& session = UserSession::instance();
    int index = str.find(':');
    if (index == -1) {
        cerr << "groupchat command invalid!" << endl;
        return;
    }
    int groupId = atoi(str.substr(0, index).c_str());
    string msg = str.substr(index + 1);
    
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = session.getCurrentUser().getId();
    js["from"] = session.getCurrentUser().getName();
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
    auto& session = UserSession::instance();
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = session.getCurrentUser().getId();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (len <= 0) {
        cerr << "failed to send logout msg: " << buffer << endl;
    } else {
        session.setMainMenuRunning(false);
        // 清空缓存
        session.clear();
    }
}
