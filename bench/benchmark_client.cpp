#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "json.hpp"
#include "public.hpp"

using json = nlohmann::json;
using namespace std;

// 读取 n 字节(循环处理半包), 返回实际读取字节数, 失败返回 -1
int recvn(int fd, void* buf, size_t n)
{
    size_t left = n;
    char* p = static_cast<char*>(buf);
    while (left > 0) {
        ssize_t r = recv(fd, p, left, 0);
        if (r <= 0) {
            return -1;
        }
        p += r;
        left -= static_cast<size_t>(r);
    }
    return static_cast<int>(n);
}

// 连接服务器, 成功返回 fd, 失败返回 -1
int connectToServer(const char* ip, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// 发送数据包: [4字节长度头(网络字节序) + JSON 体]
bool sendPacket(int fd, const string& msg)
{
    uint32_t len = htonl(static_cast<uint32_t>(msg.size()));
    string packet(reinterpret_cast<const char*>(&len), sizeof(len));
    packet += msg;
    ssize_t ret = send(fd, packet.data(), packet.size(), 0);
    return ret == static_cast<ssize_t>(packet.size());
}

// 接收一个数据包: [4字节长度头 + JSON 体], 失败返回 false
bool recvPacket(int fd, string& body)
{
    uint32_t len = 0;
    if (recvn(fd, &len, sizeof(len)) != static_cast<int>(sizeof(len))) {
        return false;
    }
    len = ntohl(len);
    if (len == 0 || len > (1u << 20)) {
        return false;
    }
    body.resize(len);
    if (recvn(fd, &body[0], len) != static_cast<int>(len)) {
        return false;
    }
    return true;
}

// 注册(唯一用户名)并登录, 成功返回用户 id, 失败返回 -1
int signupAndLogin(int fd, const string& name)
{
    // 注册
    json js;
    js["msgid"] = SIGNUP_MSG;
    js["name"] = name;
    js["password"] = "bench123";
    if (!sendPacket(fd, js.dump())) {
        return -1;
    }
    string body;
    if (!recvPacket(fd, body)) {
        return -1;
    }
    json resp = json::parse(body);
    if (resp.value("errno", -1) != 0) {
        return -1;
    }
    int id = resp.value("id", -1);

    // 登录
    json login;
    login["msgid"] = LOGIN_MSG;
    login["id"] = id;
    login["password"] = "bench123";
    if (!sendPacket(fd, login.dump())) {
        return -1;
    }
    if (!recvPacket(fd, body)) {
        return -1;
    }
    resp = json::parse(body);
    if (resp.value("errno", -1) != 0) {
        return -1;
    }
    return id;
}

// 唯一用户后缀(时间戳), 避免重名
string makeSuffix()
{
    return to_string(chrono::system_clock::now().time_since_epoch().count());
}

// 模式1: 并发连接, 测连接建立速率与并发连接能力
int benchConn(const char* ip, uint16_t port, int n)
{
    atomic<int> ok{0};
    vector<int> fds(static_cast<size_t>(n), -1);
    vector<thread> threads;

    auto t0 = chrono::steady_clock::now();
    for (int i = 0; i < n; i++) {
        threads.emplace_back([&, i] {
            fds[static_cast<size_t>(i)] = connectToServer(ip, port);
            if (fds[static_cast<size_t>(i)] >= 0) {
                ok++;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    auto t1 = chrono::steady_clock::now();
    double sec = chrono::duration<double>(t1 - t0).count();

    cout << "===== 并发连接 =====" << endl;
    cout << "连接成功: " << ok.load() << "/" << n << endl;
    cout << "耗时: " << sec << "s, 建立速率: " << (sec > 0 ? ok.load() / sec : 0) << " conn/s" << endl;

    // 保持连接 5s, 观察服务端是否稳定(服务端默认 4 个 sub-reactor)
    cout << "保持连接 5s..." << endl;
    this_thread::sleep_for(chrono::seconds(5));

    for (int fd : fds) {
        if (fd >= 0) {
            close(fd);
        }
    }
    return 0;
}

// 模式2: 并发注册+登录, 测登录吞吐(QPS)
int benchLogin(const char* ip, uint16_t port, int n)
{
    atomic<int> success{0};
    atomic<int> fail{0};
    string suffix = makeSuffix();
    vector<thread> threads;

    auto t0 = chrono::steady_clock::now();
    for (int i = 0; i < n; i++) {
        threads.emplace_back([&, i] {
            int fd = connectToServer(ip, port);
            if (fd < 0) {
                fail++;
                return;
            }
            string name = "bench" + suffix + "_" + to_string(i);
            if (signupAndLogin(fd, name) >= 0) {
                success++;
            } else {
                fail++;
            }
            close(fd);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    auto t1 = chrono::steady_clock::now();
    double sec = chrono::duration<double>(t1 - t0).count();

    cout << "===== 登录吞吐 =====" << endl;
    cout << "成功: " << success.load() << ", 失败: " << fail.load() << endl;
    cout << "耗时: " << sec << "s, 登录 QPS: " << (sec > 0 ? success.load() / sec : 0) << " login/s" << endl;
    return 0;
}

// 模式3: 单聊消息吞吐, 测本地直发路径的消息 TPS
int benchChat(const char* ip, uint16_t port, int senders, int msgsPerSender)
{
    string suffix = makeSuffix();

    // 接收者: 注册+登录, 保持连接, 后台线程计数
    int recvFd = connectToServer(ip, port);
    if (recvFd < 0) {
        cerr << "接收者连接失败" << endl;
        return 1;
    }
    int recvId = signupAndLogin(recvFd, "bench_recv_" + suffix);
    if (recvId < 0) {
        cerr << "接收者登录失败" << endl;
        close(recvFd);
        return 1;
    }
    atomic<long> received{0};
    thread recvThread([&] {
        string body;
        while (recvPacket(recvFd, body)) {
            try {
                json js = json::parse(body);
                if (js.value("msgid", -1) == ONE_CHAT_MSG) {
                    received++;
                }
            } catch (const std::exception&) {
                // 忽略解析异常
            }
        }
    });

    // 发送端: 并发向接收者发消息
    atomic<long> sent{0};
    atomic<long> sendFail{0};
    vector<thread> threads;

    auto t0 = chrono::steady_clock::now();
    for (int i = 0; i < senders; i++) {
        threads.emplace_back([&, i, recvId] {
            int fd = connectToServer(ip, port);
            if (fd < 0) {
                sendFail++;
                return;
            }
            string name = "bench_send_" + suffix + "_" + to_string(i);
            int id = signupAndLogin(fd, name);
            if (id < 0) {
                sendFail++;
                close(fd);
                return;
            }
            for (int m = 0; m < msgsPerSender; m++) {
                json js;
                js["msgid"] = ONE_CHAT_MSG;
                js["id"] = id;
                js["from"] = name;
                js["to"] = recvId;
                js["msg"] = "bench message " + to_string(m);
                js["time"] = "2026-09-15 00:00:00";
                if (sendPacket(fd, js.dump())) {
                    sent++;
                } else {
                    sendFail++;
                    break;
                }
            }
            close(fd);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    auto t1 = chrono::steady_clock::now();
    double sec = chrono::duration<double>(t1 - t0).count();

    // 等待接收者收完尾部消息
    this_thread::sleep_for(chrono::seconds(1));

    long totalSent = sent.load();
    long totalRecv = received.load();
    cout << "===== 单聊消息吞吐 =====" << endl;
    cout << "发送端: " << senders << ", 每端消息: " << msgsPerSender << endl;
    cout << "发送: " << totalSent << " 条, 接收: " << totalRecv << " 条, 发送失败: " << sendFail.load() << endl;
    cout << "耗时: " << sec << "s" << endl;
    cout << "发送 TPS: " << (sec > 0 ? totalSent / sec : 0) << " msg/s" << endl;
    cout << "接收 TPS: " << (sec > 0 ? totalRecv / sec : 0) << " msg/s" << endl;

    close(recvFd);
    recvThread.detach();
    return 0;
}

void usage()
{
    cout << "用法:" << endl;
    cout << "  bench conn  <ip> <port> <连接数>" << endl;
    cout << "  bench login <ip> <port> <客户端数>" << endl;
    cout << "  bench chat  <ip> <port> <发送端数> <每端消息数>" << endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }
    string mode = argv[1];
    if (mode == "conn" && argc == 5) {
        return benchConn(argv[2], static_cast<uint16_t>(atoi(argv[3])), atoi(argv[4]));
    }
    if (mode == "login" && argc == 5) {
        return benchLogin(argv[2], static_cast<uint16_t>(atoi(argv[3])), atoi(argv[4]));
    }
    if (mode == "chat" && argc == 6) {
        return benchChat(argv[2], static_cast<uint16_t>(atoi(argv[3])), atoi(argv[4]), atoi(argv[5]));
    }
    usage();
    return 1;
}
