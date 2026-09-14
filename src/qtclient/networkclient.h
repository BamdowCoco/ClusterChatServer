#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>
#include <QTcpSocket>

#include <string>
#include <vector>

#include "json.hpp"
#include "user.hpp"
#include "group.hpp"

// 网络客户端核心：封装 QTcpSocket，负责协议打包/解包与消息分发
// 与终端客户端 (src/client) 的职责对应：
//   - 发送：JSON + '\0'（与终端 send(..., size()+1, 0) 一致）
//   - 接收：4 字节大端长度头 + JSON 体（处理粘包/半包）
class NetworkClient : public QObject
{
    Q_OBJECT
public:
    explicit NetworkClient(QObject* parent = nullptr);

    // 连接服务器
    void connectToServer(const QString& ip, quint16 port);
    void disconnectFromServer();

    // 发送各类请求（构造 JSON 后经 sendJson 发出）
    void sendLogin(int id, const QString& password);
    void sendSignup(const QString& name, const QString& password);
    void sendChat(int to, const QString& msg);
    void sendAddFriend(int friendId);
    void sendCreateGroup(const QString& name, const QString& desc);
    void sendJoinGroup(int groupId);
    void sendGroupChat(int groupId, const QString& msg);
    void sendLogout();

    // 访问器：登录成功后由 UI 拉取
    const User& currentUser() const { return _currentUser; }
    const std::vector<User>& friendList() const { return _friendList; }
    const std::vector<Group>& groupList() const { return _groupList; }

signals:
    void connected();
    void disconnected();
    void connectError(const QString& err);

    void loginResult(bool success, const QString& msg);
    void signupResult(bool success, const QString& msg);
    void addFriendResult(bool success, const QString& msg);
    void createGroupResult(bool success, const QString& msg);
    void joinGroupResult(bool success, const QString& msg);
    void logoutResult(bool success);

    void chatReceived(int fromId, const QString& fromName, const QString& msg, const QString& time);
    void groupChatReceived(int groupId, int fromId, const QString& fromName, const QString& msg, const QString& time);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    // 发送 JSON 字符串（客户端 -> 服务端格式：JSON + '\0'）
    void sendJson(const std::string& jsonStr);
    // 解析并分发服务端消息（服务端 -> 客户端格式：长度头 + JSON）
    void handleMessage(const std::string& jsonStr);
    void handleLoginResponse(const nlohmann::json& js);
    void handleSignupResponse(const nlohmann::json& js);

    QTcpSocket* _socket;
    QByteArray _buffer; // 接收缓冲，用于处理 TCP 粘包/半包

    User _currentUser;
    std::vector<User> _friendList;
    std::vector<Group> _groupList;
};

#endif // NETWORKCLIENT_H
