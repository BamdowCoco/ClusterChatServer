#include "networkclient.h"

#include <QDateTime>
#include <QtEndian>

#include "json.hpp"
#include "public.hpp"

using json = nlohmann::json;

NetworkClient::NetworkClient(QObject* parent)
    : QObject(parent)
    , _socket(new QTcpSocket(this))
{
    connect(_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &NetworkClient::onReadyRead);
    connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &NetworkClient::onSocketError);
}

void NetworkClient::connectToServer(const QString& ip, quint16 port)
{
    _buffer.clear();
    _socket->connectToHost(ip, port);
}

void NetworkClient::disconnectFromServer()
{
    _socket->disconnectFromHost();
}

void NetworkClient::sendJson(const std::string& jsonStr)
{
    if (_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    // 与终端客户端/服务端保持一致：客户端 -> 服务端 发送 [4字节大端长度头 + JSON 体]
    quint32 len = qToBigEndian<quint32>(static_cast<quint32>(jsonStr.size()));
    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(&len), sizeof(len));
    packet.append(jsonStr.c_str(), static_cast<int>(jsonStr.size()));
    _socket->write(packet);
}

void NetworkClient::sendLogin(int id, const QString& password)
{
    json js;
    js["msgid"] = LOGIN_MSG;
    js["id"] = id;
    js["password"] = password.toStdString();
    sendJson(js.dump());
}

void NetworkClient::sendSignup(const QString& name, const QString& password)
{
    json js;
    js["msgid"] = SIGNUP_MSG;
    js["name"] = name.toStdString();
    js["password"] = password.toStdString();
    sendJson(js.dump());
}

void NetworkClient::sendChat(int to, const QString& msg)
{
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = _currentUser.getId();
    js["from"] = _currentUser.getName();
    js["to"] = to;
    js["msg"] = msg.toStdString();
    js["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    sendJson(js.dump());
}

void NetworkClient::sendAddFriend(int friendId)
{
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = _currentUser.getId();
    js["friendid"] = friendId;
    sendJson(js.dump());
}

void NetworkClient::sendCreateGroup(const QString& name, const QString& desc)
{
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = _currentUser.getId();
    js["name"] = name.toStdString();
    js["desc"] = desc.toStdString();
    sendJson(js.dump());
}

void NetworkClient::sendJoinGroup(int groupId)
{
    json js;
    js["msgid"] = JOIN_GROUP_MSG;
    js["groupid"] = groupId;
    js["userid"] = _currentUser.getId();
    js["role"] = "normal";
    sendJson(js.dump());
}

void NetworkClient::sendGroupChat(int groupId, const QString& msg)
{
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = _currentUser.getId();
    js["from"] = _currentUser.getName();
    js["groupid"] = groupId;
    js["msg"] = msg.toStdString();
    js["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    sendJson(js.dump());
}

void NetworkClient::sendLogout()
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = _currentUser.getId();
    sendJson(js.dump());
}

void NetworkClient::onConnected()
{
    emit connected();
}

void NetworkClient::onDisconnected()
{
    emit disconnected();
}

void NetworkClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit connectError(_socket->errorString());
}

void NetworkClient::onReadyRead()
{
    _buffer.append(_socket->readAll());

    // 循环解帧：处理 TCP 粘包（多帧连在一起）与半包（一帧未收全）
    while (true) {
        if (_buffer.size() < 4) {
            // 不足 4 字节，无法确定长度，等待更多数据
            return;
        }
        quint32 len = qFromBigEndian<quint32>(_buffer.constData());
        if (_buffer.size() < 4 + static_cast<int>(len)) {
            // 数据体未收全（半包），等待更多数据
            return;
        }
        QByteArray body = _buffer.mid(4, static_cast<int>(len));
        _buffer.remove(0, 4 + static_cast<int>(len));
        handleMessage(std::string(body.constData(), static_cast<size_t>(body.size())));
    }
}

void NetworkClient::handleMessage(const std::string& jsonStr)
{
    json js = json::parse(jsonStr);
    switch (js["msgid"].get<int>()) {
    case ONE_CHAT_MSG: {
        emit chatReceived(js["id"].get<int>(),
                          QString::fromStdString(js["from"].get<std::string>()),
                          QString::fromStdString(js["msg"].get<std::string>()),
                          QString::fromStdString(js["time"].get<std::string>()));
        break;
    }
    case GROUP_CHAT_MSG: {
        emit groupChatReceived(js["groupid"].get<int>(),
                               js["id"].get<int>(),
                               QString::fromStdString(js["from"].get<std::string>()),
                               QString::fromStdString(js["msg"].get<std::string>()),
                               QString::fromStdString(js["time"].get<std::string>()));
        break;
    }
    case LOGIN_MSG_ACK:
        handleLoginResponse(js);
        break;
    case SIGNUP_MSG_ACK:
        handleSignupResponse(js);
        break;
    case ADD_FRIEND_MSG_ACK: {
        bool ok = js["errno"].get<int>() == 0;
        QString msg = js.contains("errmsg")
            ? QString::fromStdString(js["errmsg"].get<std::string>())
            : QString();
        emit addFriendResult(ok, msg);
        break;
    }
    case CREATE_GROUP_MSG_ACK: {
        bool ok = js["errno"].get<int>() == 0;
        QString msg = js.contains("errmsg")
            ? QString::fromStdString(js["errmsg"].get<std::string>())
            : QString();
        emit createGroupResult(ok, msg);
        break;
    }
    case JOIN_GROUP_MSG_ACK: {
        bool ok = js["errno"].get<int>() == 0;
        QString msg = js.contains("errmsg")
            ? QString::fromStdString(js["errmsg"].get<std::string>())
            : QString();
        emit joinGroupResult(ok, msg);
        break;
    }
    case LOGOUT_MSG_ACK: {
        emit logoutResult(js["errno"].get<int>() == 0);
        break;
    }
    default:
        break;
    }
}

void NetworkClient::handleLoginResponse(const json& js)
{
    if (js["errno"].get<int>() != 0) {
        QString msg = js.contains("errmsg")
            ? QString::fromStdString(js["errmsg"].get<std::string>())
            : QString("登录失败");
        emit loginResult(false, msg);
        return;
    }

    // 保存当前用户
    _currentUser = User(js["id"].get<int>(), js["name"].get<std::string>(), "", "online");
    _friendList.clear();
    _groupList.clear();

    // 反序列化好友列表
    if (js.contains("friends")) {
        for (const auto& str : js["friends"]) {
            json f = json::parse(str.get<std::string>());
            User u;
            u.setId(f["id"].get<int>());
            u.setName(f["name"].get<std::string>());
            u.setState(f["state"].get<std::string>());
            _friendList.push_back(u);
        }
    }

    // 反序列化群组列表
    if (js.contains("groups")) {
        for (const auto& str : js["groups"]) {
            json g = json::parse(str.get<std::string>());
            Group group;
            group.setId(g["id"].get<int>());
            group.setGroupName(g["name"].get<std::string>());
            group.setGroupDesc(g["desc"].get<std::string>());
            for (const auto& mstr : g["members"]) {
                json m = json::parse(mstr.get<std::string>());
                GroupUser gu;
                gu.setId(m["id"].get<int>());
                gu.setName(m["name"].get<std::string>());
                gu.setState(m["state"].get<std::string>());
                gu.setGroupRole(m["role"].get<std::string>());
                group.getUsers().push_back(gu);
            }
            _groupList.push_back(group);
        }
    }

    // 先通知登录成功，UI 据此填充好友/群组列表
    emit loginResult(true, QString());

    // 补发离线消息（登录成功、MainWindow 建立信号连接后触发）
    if (js.contains("offlinemsg")) {
        for (const auto& str : js["offlinemsg"]) {
            json m = json::parse(str.get<std::string>());
            if (m["msgid"].get<int>() == ONE_CHAT_MSG) {
                emit chatReceived(m["id"].get<int>(),
                                  QString::fromStdString(m["from"].get<std::string>()),
                                  QString::fromStdString(m["msg"].get<std::string>()),
                                  QString::fromStdString(m["time"].get<std::string>()));
            } else if (m["msgid"].get<int>() == GROUP_CHAT_MSG) {
                emit groupChatReceived(m["groupid"].get<int>(),
                                       m["id"].get<int>(),
                                       QString::fromStdString(m["from"].get<std::string>()),
                                       QString::fromStdString(m["msg"].get<std::string>()),
                                       QString::fromStdString(m["time"].get<std::string>()));
            }
        }
    }
}

void NetworkClient::handleSignupResponse(const json& js)
{
    bool ok = js["errno"].get<int>() == 0;
    QString name = QString::fromStdString(js["name"].get<std::string>());
    QString msg;
    if (ok) {
        msg = QString("注册成功，用户ID: %1（请牢记）").arg(js["id"].get<int>());
    } else {
        msg = QString("注册失败：用户 %1 已存在").arg(name);
    }
    emit signupResult(ok, msg);
}
