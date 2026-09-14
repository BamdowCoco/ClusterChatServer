#include "mainwindow.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "networkclient.h"
#include "group.hpp"

MainWindow::MainWindow(NetworkClient* client, QWidget* parent)
    : QMainWindow(parent)
    , _client(client)
    , _chatFriendId(-1)
    , _chatGroupId(-1)
{
    setWindowTitle("集群聊天客户端");
    resize(800, 560);

    // ---- 顶部：用户信息 + 注销 ----
    _userLabel = new QLabel(this);
    _logoutBtn = new QPushButton("注销", this);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addWidget(_userLabel);
    topLayout->addStretch();
    topLayout->addWidget(_logoutBtn);

    // ---- 左侧：好友/群组列表 ----
    _friendListWidget = new QListWidget(this);
    _groupListWidget = new QListWidget(this);
    _tabWidget = new QTabWidget(this);
    _tabWidget->addTab(_friendListWidget, "好友");
    _tabWidget->addTab(_groupListWidget, "群组");

    // ---- 右侧：聊天区 ----
    _targetLabel = new QLabel("请选择聊天对象", this);
    _chatBrowser = new QTextBrowser(this);
    _inputEdit = new QLineEdit(this);
    _sendBtn = new QPushButton("发送", this);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(_inputEdit);
    inputLayout->addWidget(_sendBtn);

    // ---- 操作按钮 ----
    _addFriendBtn = new QPushButton("添加好友", this);
    _createGroupBtn = new QPushButton("创建群组", this);
    _joinGroupBtn = new QPushButton("加入群组", this);

    QHBoxLayout* opLayout = new QHBoxLayout();
    opLayout->addWidget(_addFriendBtn);
    opLayout->addWidget(_createGroupBtn);
    opLayout->addWidget(_joinGroupBtn);
    opLayout->addStretch();

    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(_targetLabel);
    rightLayout->addWidget(_chatBrowser);
    rightLayout->addLayout(inputLayout);
    rightLayout->addLayout(opLayout);

    QWidget* rightWidget = new QWidget(this);
    rightWidget->setLayout(rightLayout);

    // ---- 整体布局：顶部 + (左侧 | 右侧) ----
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(_tabWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    QWidget* central = new QWidget(this);
    QVBoxLayout* centralLayout = new QVBoxLayout(central);
    centralLayout->addLayout(topLayout);
    centralLayout->addWidget(splitter);
    setCentralWidget(central);

    // ---- 填充当前用户信息与好友/群组列表 ----
    const User& me = _client->currentUser();
    _userLabel->setText(QString("当前用户: %1 (ID: %2)").arg(QString::fromStdString(me.getName())).arg(me.getId()));

    for (const User& f : _client->friendList()) {
        QListWidgetItem* item = new QListWidgetItem(
            QString("%1 (ID: %2)").arg(QString::fromStdString(f.getName())).arg(f.getId()));
        item->setData(Qt::UserRole, f.getId());
        _friendListWidget->addItem(item);
    }
    for (const Group& g : _client->groupList()) {
        QListWidgetItem* item = new QListWidgetItem(
            QString("%1 (ID: %2)").arg(QString::fromStdString(g.getGroupName())).arg(g.getId()));
        item->setData(Qt::UserRole, g.getId());
        _groupListWidget->addItem(item);
    }

    // ---- 信号槽连接 ----
    connect(_logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
    connect(_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(_inputEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);
    connect(_addFriendBtn, &QPushButton::clicked, this, &MainWindow::onAddFriendClicked);
    connect(_createGroupBtn, &QPushButton::clicked, this, &MainWindow::onCreateGroupClicked);
    connect(_joinGroupBtn, &QPushButton::clicked, this, &MainWindow::onJoinGroupClicked);

    connect(_friendListWidget, &QListWidget::itemClicked, this, &MainWindow::onFriendSelected);
    connect(_groupListWidget, &QListWidget::itemClicked, this, &MainWindow::onGroupSelected);

    connect(_client, &NetworkClient::chatReceived, this, &MainWindow::onChatReceived);
    connect(_client, &NetworkClient::groupChatReceived, this, &MainWindow::onGroupChatReceived);
    connect(_client, &NetworkClient::addFriendResult, this, &MainWindow::onAddFriendResult);
    connect(_client, &NetworkClient::createGroupResult, this, &MainWindow::onCreateGroupResult);
    connect(_client, &NetworkClient::joinGroupResult, this, &MainWindow::onJoinGroupResult);
}

void MainWindow::appendChat(const QString& text)
{
    _chatBrowser->append(text);
}

void MainWindow::onChatReceived(int fromId, const QString& fromName, const QString& msg, const QString& time)
{
    appendChat(QString("[%1] [%2]%3 说: %4").arg(time).arg(fromId).arg(fromName).arg(msg));
}

void MainWindow::onGroupChatReceived(int groupId, int fromId, const QString& fromName, const QString& msg, const QString& time)
{
    appendChat(QString("群消息[%1]: [%2] [%3]%4 说: %5")
                   .arg(groupId)
                   .arg(time)
                   .arg(fromId)
                   .arg(fromName)
                   .arg(msg));
}

void MainWindow::onFriendSelected(QListWidgetItem* item)
{
    _chatFriendId = item->data(Qt::UserRole).toInt();
    _chatGroupId = -1;
    _targetLabel->setText(QString("私聊对象: %1").arg(item->text()));
    _inputEdit->setFocus();
}

void MainWindow::onGroupSelected(QListWidgetItem* item)
{
    _chatGroupId = item->data(Qt::UserRole).toInt();
    _chatFriendId = -1;
    _targetLabel->setText(QString("群聊对象: %1").arg(item->text()));
    _inputEdit->setFocus();
}

void MainWindow::onSendClicked()
{
    QString msg = _inputEdit->text();
    if (msg.isEmpty()) {
        return;
    }
    if (_chatGroupId != -1) {
        _client->sendGroupChat(_chatGroupId, msg);
    } else if (_chatFriendId != -1) {
        _client->sendChat(_chatFriendId, msg);
    } else {
        QMessageBox::information(this, "提示", "请先在左侧选择好友或群组");
        return;
    }
    _inputEdit->clear();
}

void MainWindow::onAddFriendClicked()
{
    bool ok = false;
    int friendId = QInputDialog::getInt(this, "添加好友", "好友ID:", 0, 1, 2147483647, 1, &ok);
    if (ok) {
        _client->sendAddFriend(friendId);
    }
}

void MainWindow::onCreateGroupClicked()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "创建群组", "群名:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    QString desc = QInputDialog::getText(this, "创建群组", "群描述:", QLineEdit::Normal, "", &ok);
    if (!ok) {
        return;
    }
    _client->sendCreateGroup(name, desc);
}

void MainWindow::onJoinGroupClicked()
{
    bool ok = false;
    int groupId = QInputDialog::getInt(this, "加入群组", "群ID:", 0, 1, 2147483647, 1, &ok);
    if (ok) {
        _client->sendJoinGroup(groupId);
    }
}

void MainWindow::onLogoutClicked()
{
    _client->sendLogout();
    // 与终端客户端一致：发送后立即返回登录界面，不等待 ACK
    emit logoutDone();
}

void MainWindow::onAddFriendResult(bool success, const QString& msg)
{
    if (success) {
        QMessageBox::information(this, "添加好友", "添加成功");
    } else {
        QMessageBox::warning(this, "添加好友", msg.isEmpty() ? "添加失败" : msg);
    }
}

void MainWindow::onCreateGroupResult(bool success, const QString& msg)
{
    if (success) {
        QMessageBox::information(this, "创建群组", "创建成功");
    } else {
        QMessageBox::warning(this, "创建群组", msg.isEmpty() ? "创建失败" : msg);
    }
}

void MainWindow::onJoinGroupResult(bool success, const QString& msg)
{
    if (success) {
        QMessageBox::information(this, "加入群组", "加入成功");
    } else {
        QMessageBox::warning(this, "加入群组", msg.isEmpty() ? "加入失败" : msg);
    }
}
