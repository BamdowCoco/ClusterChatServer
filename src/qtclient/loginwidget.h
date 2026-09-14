#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class NetworkClient;

// 登录/注册窗口：包含服务器地址配置、登录区、注册区
class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(NetworkClient* client, QWidget* parent = nullptr);

signals:
    void loginSucceeded();

private slots:
    void onConnectClicked();
    void onLoginClicked();
    void onSignupClicked();
    void onLoginResult(bool success, const QString& msg);
    void onSignupResult(bool success, const QString& msg);
    void onConnected();
    void onConnectError(const QString& err);

private:
    NetworkClient* _client;

    // 服务器配置
    QLineEdit* _ipEdit;
    QLineEdit* _portEdit;
    QPushButton* _connectBtn;

    // 登录区
    QLineEdit* _loginIdEdit;
    QLineEdit* _loginPwdEdit;
    QPushButton* _loginBtn;

    // 注册区
    QLineEdit* _signupNameEdit;
    QLineEdit* _signupPwdEdit;
    QPushButton* _signupBtn;

    QLabel* _statusLabel;
};

#endif // LOGINWIDGET_H
