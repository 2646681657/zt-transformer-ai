#ifndef USERSTORE_H
#define USERSTORE_H
// 用户认证存储（管理登录状态和凭证验证）

#include <QString>

class UserStore {
public:
    // 验证用户名密码，成功后记录当前登录用户
    bool authenticate(const QString &username, const QString &password);
    QString currentUser() const { return m_currentUser; }
    void logout() { m_currentUser.clear(); }
    bool isLoggedIn() const { return !m_currentUser.isEmpty(); }

private:
    QString m_currentUser;
};

#endif // USERSTORE_H
