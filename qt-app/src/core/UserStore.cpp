#include "UserStore.h"

bool UserStore::authenticate(const QString &username, const QString &password)
{
    if ((username == "admin" && password == "123456") ||
        (username == "user" && password == "123456")) {
        m_currentUser = username;
        return true;
    }
    return false;
}
