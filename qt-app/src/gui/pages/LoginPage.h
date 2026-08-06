#ifndef LOGINPAGE_H
#define LOGINPAGE_H
// 登录页（用户名密码验证，支持记住密码）

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QSettings>

class UserStore;

class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(UserStore *store, QWidget *parent = nullptr);

signals:
    void loginSuccess();

private slots:
    void onLoginClicked();

private:
    UserStore *m_store;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLabel *m_errorLabel;
    QCheckBox *m_rememberCheck;
    QSettings m_settings;
};

#endif // LOGINPAGE_H
