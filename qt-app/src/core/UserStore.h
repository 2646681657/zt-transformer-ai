#ifndef USERSTORE_H
#define USERSTORE_H
// 用户认证存储（持久化用户管理：JSON 存储、增删改查、角色区分）
// 管理员可增删用户和修改密码，普通用户只能修改自己的密码

#include <QString>
#include <QVector>

struct UserInfo {
    QString username;
    QString password;    // 明文存储（内网工具，简化实现）
    QString role;        // "admin" 或 "user"
    QString displayName; // 显示名称（可选）
};

class UserStore {
public:
    static UserStore &instance();

    // 验证用户名密码，成功后记录当前登录用户
    bool authenticate(const QString &username, const QString &password);
    QString currentUser() const { return m_currentUser; }
    void logout() { m_currentUser.clear(); }
    bool isLoggedIn() const { return !m_currentUser.isEmpty(); }

    // 当前用户是否为管理员
    bool isAdmin() const;

    // ---- 用户管理（管理员权限）----
    // 获取全部用户列表
    QVector<UserInfo> users() const { return m_users; }
    // 添加用户（管理员），成功返回 true
    bool addUser(const QString &username, const QString &password,
                 const QString &role, const QString &displayName = QString());
    // 删除用户（管理员，不能删除自己或最后一个管理员）
    bool deleteUser(const QString &username);
    // 修改密码（用户可改自己，管理员可改任何人）
    bool changePassword(const QString &username, const QString &newPassword);
    // 修改用户信息（管理员）
    bool updateUser(const QString &username, const QString &newRole,
                    const QString &newDisplayName);

    QString lastError() const { return m_lastError; }

private:
    UserStore();

    // 从 JSON 文件加载用户列表；不存在则初始化默认账号
    void load();
    // 保存到 JSON 文件
    bool save();

    QString m_currentUser;
    QVector<UserInfo> m_users;
    QString m_lastError;
};

#endif // USERSTORE_H
