#include "UserStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

UserStore &UserStore::instance()
{
    static UserStore store;
    return store;
}

UserStore::UserStore()
{
    load();
}

void UserStore::load()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    const QString path = appData + QStringLiteral("/users.json");

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // 首次运行：初始化默认账号
        m_users.append({QStringLiteral("admin"), QStringLiteral("123456"),
                         QStringLiteral("admin"), QStringLiteral("管理员")});
        m_users.append({QStringLiteral("user"), QStringLiteral("123456"),
                         QStringLiteral("user"), QStringLiteral("普通用户")});
        save();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    m_users.clear();
    const QJsonArray arr = doc.array();
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        UserInfo u;
        u.username = o.value("username").toString();
        u.password = o.value("password").toString();
        u.role = o.value("role").toString("user");
        u.displayName = o.value("displayName").toString();
        if (!u.username.isEmpty()) {
            m_users.append(u);
        }
    }

    // 如果文件损坏导致列表为空，恢复默认
    if (m_users.isEmpty()) {
        m_users.append({QStringLiteral("admin"), QStringLiteral("123456"),
                         QStringLiteral("admin"), QStringLiteral("管理员")});
        save();
    }
}

bool UserStore::save()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + QStringLiteral("/users.json");

    QJsonArray arr;
    for (const auto &u : m_users) {
        QJsonObject o;
        o.insert("username", u.username);
        o.insert("password", u.password);
        o.insert("role", u.role);
        o.insert("displayName", u.displayName);
        arr.append(o);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QStringLiteral("无法写入用户文件");
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}

bool UserStore::authenticate(const QString &username, const QString &password)
{
    for (const auto &u : m_users) {
        if (u.username == username && u.password == password) {
            m_currentUser = username;
            return true;
        }
    }
    return false;
}

bool UserStore::isAdmin() const
{
    for (const auto &u : m_users) {
        if (u.username == m_currentUser) {
            return u.role == "admin";
        }
    }
    return false;
}

bool UserStore::addUser(const QString &username, const QString &password,
                        const QString &role, const QString &displayName)
{
    if (username.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("用户名不能为空");
        return false;
    }
    if (password.isEmpty()) {
        m_lastError = QStringLiteral("密码不能为空");
        return false;
    }
    // 检查重名
    for (const auto &u : m_users) {
        if (u.username == username) {
            m_lastError = QStringLiteral("用户名已存在: %1").arg(username);
            return false;
        }
    }
    m_users.append({username, password,
                    role == "admin" ? "admin" : "user", displayName});
    return save();
}

bool UserStore::deleteUser(const QString &username)
{
    if (username == m_currentUser) {
        m_lastError = QStringLiteral("不能删除当前登录用户");
        return false;
    }
    // 不能删除最后一个管理员
    int adminCount = 0;
    int targetIdx = -1;
    for (int i = 0; i < m_users.size(); ++i) {
        if (m_users[i].role == "admin") ++adminCount;
        if (m_users[i].username == username) targetIdx = i;
    }
    if (targetIdx < 0) {
        m_lastError = QStringLiteral("用户不存在: %1").arg(username);
        return false;
    }
    if (m_users[targetIdx].role == "admin" && adminCount <= 1) {
        m_lastError = QStringLiteral("不能删除最后一个管理员");
        return false;
    }
    m_users.removeAt(targetIdx);
    return save();
}

bool UserStore::changePassword(const QString &username, const QString &newPassword)
{
    if (newPassword.isEmpty()) {
        m_lastError = QStringLiteral("密码不能为空");
        return false;
    }
    for (auto &u : m_users) {
        if (u.username == username) {
            u.password = newPassword;
            return save();
        }
    }
    m_lastError = QStringLiteral("用户不存在: %1").arg(username);
    return false;
}

bool UserStore::updateUser(const QString &username, const QString &newRole,
                            const QString &newDisplayName)
{
    // 检查降级最后一个管理员
    if (newRole != "admin") {
        int adminCount = 0;
        int targetIdx = -1;
        for (int i = 0; i < m_users.size(); ++i) {
            if (m_users[i].role == "admin") ++adminCount;
            if (m_users[i].username == username) targetIdx = i;
        }
        if (targetIdx >= 0 && m_users[targetIdx].role == "admin" && adminCount <= 1) {
            m_lastError = QStringLiteral("不能降级最后一个管理员");
            return false;
        }
    }

    for (auto &u : m_users) {
        if (u.username == username) {
            u.role = newRole == "admin" ? "admin" : "user";
            u.displayName = newDisplayName;
            return save();
        }
    }
    m_lastError = QStringLiteral("用户不存在: %1").arg(username);
    return false;
}
