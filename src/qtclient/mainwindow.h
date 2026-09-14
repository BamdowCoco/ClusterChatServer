#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>

class QListWidget;
class QTextBrowser;
class QLineEdit;
class QPushButton;
class QLabel;
class QTabWidget;
class NetworkClient;

// 聊天主窗口：好友/群组列表 + 聊天区 + 输入框
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(NetworkClient* client, QWidget* parent = nullptr);

signals:
    void logoutDone();

private slots:
    void onChatReceived(int fromId, const QString& fromName, const QString& msg, const QString& time);
    void onGroupChatReceived(int groupId, int fromId, const QString& fromName, const QString& msg, const QString& time);
    void onFriendSelected(QListWidgetItem* item);
    void onGroupSelected(QListWidgetItem* item);
    void onSendClicked();
    void onAddFriendClicked();
    void onCreateGroupClicked();
    void onJoinGroupClicked();
    void onLogoutClicked();
    void onAddFriendResult(bool success, const QString& msg);
    void onCreateGroupResult(bool success, const QString& msg);
    void onJoinGroupResult(bool success, const QString& msg);

private:
    void appendChat(const QString& text);

    NetworkClient* _client;

    // 顶部
    QLabel* _userLabel;
    QPushButton* _logoutBtn;

    // 左侧列表
    QTabWidget* _tabWidget;
    QListWidget* _friendListWidget;
    QListWidget* _groupListWidget;

    // 右侧聊天区
    QLabel* _targetLabel;
    QTextBrowser* _chatBrowser;
    QLineEdit* _inputEdit;
    QPushButton* _sendBtn;

    // 操作按钮
    QPushButton* _addFriendBtn;
    QPushButton* _createGroupBtn;
    QPushButton* _joinGroupBtn;

    int _chatFriendId; // 当前私聊对象 id，-1 表示无
    int _chatGroupId;  // 当前群聊对象 id，-1 表示无
};

#endif // MAINWINDOW_H
