#include <QApplication>

#include "networkclient.h"
#include "loginwidget.h"
#include "mainwindow.h"

// Qt 客户端入口：创建 NetworkClient，管理登录窗口与主窗口的切换
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    NetworkClient client;
    LoginWidget login(&client);
    MainWindow* mainWin = nullptr;

    // 登录成功 -> 创建并显示主窗口
    QObject::connect(&login, &LoginWidget::loginSucceeded, [&]() {
        mainWin = new MainWindow(&client);
        // 注销 -> 关闭主窗口，返回登录界面
        QObject::connect(mainWin, &MainWindow::logoutDone, [&]() {
            mainWin->deleteLater();
            mainWin = nullptr;
            login.show();
        });
        mainWin->show();
        login.hide();
    });

    login.show();
    return app.exec();
}
