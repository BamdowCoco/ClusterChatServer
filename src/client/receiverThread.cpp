#include "receiverThread.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "json.hpp"
using json = nlohmann::json;
using namespace std;

#include "public.hpp"
#include "session.hpp"
#include "messageHandler.hpp"

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

// 接收线程
void readTaskHandler(int clientfd)
{
    auto& session = UserSession::instance();
    while (true) {
        int ret;
        // 接收数据长度
        uint32_t len;
        ret = recv(clientfd, &len, sizeof(uint32_t), 0);
        if (ret == -1 || ret == 0) {
            // close(clientfd);
            DEBUG_LOG("close");
            exit(0);
        }
        if (ret != sizeof(uint32_t)) {
            DEBUG_LOG("failed to recv len");
            exit(1);
        }
        len = ntohl(len);
        DEBUG_LOG(len);

        // 接收原始数据
        string buffer;
        buffer.resize(len);
        ret = recv(clientfd, &buffer[0], len, 0);
        DEBUG_LOG(buffer);

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
        case LOGIN_MSG_ACK: {
            MessageHandler::handleLoginResponse(js); // 处理登录响应业务逻辑
            session.rwPost(); // 登录响应已处理 唤醒主线程
            break;
        }
        case SIGNUP_MSG_ACK: {
            MessageHandler::handleSignupResponse(js);
            session.rwPost();
            break;
        }
        default: {
            break;
        }
        }
    }
}