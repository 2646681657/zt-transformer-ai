#ifndef USERSTORE_H
#define USERSTORE_H

#include <QString>

class UserStore {
public:
    bool authenticate(const QString &username, const QString &password);
    QString currentUser() const { return m_currentUser; }
    void logout() { m_currentUser.clear(); }
    bool isLoggedIn() const { return !m_currentUser.isEmpty(); }

private:
    QString m_currentUser;
};

#endif // USERSTORE_H
