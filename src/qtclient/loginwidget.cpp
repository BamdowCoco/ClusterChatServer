#include "loginwidget.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "networkclient.h"

LoginWidget::LoginWidget(NetworkClient* client, QWidget* parent)
    : QWidget(parent)
    , _client(client)
{
    setWindowTitle(QString("集群聊天客户端 - 登录"));
    setFixedWidth(360);

    // ---- 服务器配置区 ----
    QGroupBox* serverBox = new QGroupBox("服务器配置", this);
    QGridLayout* serverLayout = new QGridLayout(serverBox);
    _ipEdit = new QLineEdit("127.0.0.1", serverBox);
    _portEdit = new QLineEdit("8000", serverBox);
    _connectBtn = new QPushButton("连接", serverBox);
    serverLayout->addWidget(new QLabel("IP:"), 0, 0);
    serverLayout->addWidget(_ipEdit, 0, 1);
    serverLayout->addWidget(new QLabel("端口:"), 0, 2);
    serverLayout->addWidget(_portEdit, 0, 3);
    serverLayout->addWidget(_connectBtn, 0, 4);

    // ---- 登录区 ----
    QGroupBox* loginBox = new QGroupBox("登录", this);
    QGridLayout* loginLayout = new QGridLayout(loginBox);
    _loginIdEdit = new QLineEdit(loginBox);
    _loginPwdEdit = new QLineEdit(loginBox);
    _loginPwdEdit->setEchoMode(QLineEdit::Password);
    _loginBtn = new QPushButton("登录", loginBox);
    loginLayout->addWidget(new QLabel("用户ID:"), 0, 0);
    loginLayout->addWidget(_loginIdEdit, 0, 1);
    loginLayout->addWidget(new QLabel("密码:"), 1, 0);
    loginLayout->addWidget(_loginPwdEdit, 1, 1);
    loginLayout->addWidget(_loginBtn, 1, 2);

    // ---- 注册区 ----
    QGroupBox* signupBox = new QGroupBox("注册", this);
    QGridLayout* signupLayout = new QGridLayout(signupBox);
    _signupNameEdit = new QLineEdit(signupBox);
    _signupPwdEdit = new QLineEdit(signupBox);
    _signupPwdEdit->setEchoMode(QLineEdit::Password);
    _signupBtn = new QPushButton("注册", signupBox);
    signupLayout->addWidget(new QLabel("用户名:"), 0, 0);
    signupLayout->addWidget(_signupNameEdit, 0, 1);
    signupLayout->addWidget(new QLabel("密码:"), 1, 0);
    signupLayout->addWidget(_signupPwdEdit, 1, 1);
    signupLayout->addWidget(_signupBtn, 1, 2);

    _statusLabel = new QLabel("请先连接服务器", this);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(serverBox);
    mainLayout->addWidget(loginBox);
    mainLayout->addWidget(signupBox);
    mainLayout->addWidget(_statusLabel);

    // 信号槽连接
    connect(_connectBtn, &QPushButton::clicked, this, &LoginWidget::onConnectClicked);
    connect(_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    connect(_signupBtn, &QPushButton::clicked, this, &LoginWidget::onSignupClicked);
    connect(_client, &NetworkClient::connected, this, &LoginWidget::onConnected);
    connect(_client, &NetworkClient::connectError, this, &LoginWidget::onConnectError);
    connect(_client, &NetworkClient::loginResult, this, &LoginWidget::onLoginResult);
    connect(_client, &NetworkClient::signupResult, this, &LoginWidget::onSignupResult);
}

void LoginWidget::onConnectClicked()
{
    QString ip = _ipEdit->text().trimmed();
    quint16 port = static_cast<quint16>(_portEdit->text().toUInt());
    _statusLabel->setText(QString("正在连接 %1:%2 ...").arg(ip).arg(port));
    _client->connectToServer(ip, port);
}

void LoginWidget::onConnected()
{
    _statusLabel->setText("已连接服务器");
}

void LoginWidget::onConnectError(const QString& err)
{
    _statusLabel->setText(QString("连接失败: %1").arg(err));
}

void LoginWidget::onLoginClicked()
{
    bool ok = false;
    int id = _loginIdEdit->text().toInt(&ok);
    if (!ok || id <= 0) {
        _statusLabel->setText("请输入有效的用户ID");
        return;
    }
    _client->sendLogin(id, _loginPwdEdit->text());
}

void LoginWidget::onSignupClicked()
{
    QString name = _signupNameEdit->text().trimmed();
    if (name.isEmpty() || _signupPwdEdit->text().isEmpty()) {
        _statusLabel->setText("用户名和密码不能为空");
        return;
    }
    _client->sendSignup(name, _signupPwdEdit->text());
}

void LoginWidget::onLoginResult(bool success, const QString& msg)
{
    if (success) {
        _statusLabel->setText("登录成功");
        emit loginSucceeded();
    } else {
        _statusLabel->setText(QString("登录失败: %1").arg(msg));
    }
}

void LoginWidget::onSignupResult(bool success, const QString& msg)
{
    _statusLabel->setText(msg);
    if (success) {
        // 注册成功后清空登录ID便于直接登录
        _loginIdEdit->clear();
    }
}
