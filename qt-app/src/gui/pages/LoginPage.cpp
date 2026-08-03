#include "LoginPage.h"
#include "UserStore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

LoginPage::LoginPage(UserStore *store, QWidget *parent)
    : QWidget(parent), m_store(store),
      m_settings("ZTF", "Designer")
{
    setStyleSheet("LoginPage { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                  "stop:0 #1e3a5f, stop:1 #2a5a9a); }");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    auto *card = new QFrame(this);
    card->setFixedSize(380, 320);
    card->setStyleSheet("QFrame { background: white; border-radius: 8px; }");

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(14);
    cardLayout->setContentsMargins(36, 28, 36, 28);

    auto *title = new QLabel(QStringLiteral("同优计算优化设计软件(V2.0)"), card);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color: #2a5a9a; font-size: 17px; font-weight: bold;"
                         "background: transparent;");
    cardLayout->addWidget(title);
    cardLayout->addSpacing(8);

    m_usernameEdit = new QLineEdit(card);
    m_usernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_usernameEdit->setFixedHeight(34);
    m_usernameEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #ccc; border-radius: 4px; padding: 4px 10px;"
        "font-size: 13px; background: #fafafa; color: #333; }"
        "QLineEdit:focus { border: 1px solid #2a5a9a; background: white; }");
    cardLayout->addWidget(m_usernameEdit);

    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setFixedHeight(34);
    m_passwordEdit->setStyleSheet(m_usernameEdit->styleSheet());
    cardLayout->addWidget(m_passwordEdit);

    m_rememberCheck = new QCheckBox(QStringLiteral("记住账号密码"), card);
    m_rememberCheck->setStyleSheet("QCheckBox { color: #555; font-size: 12px;"
                                   "background: transparent; }");
    cardLayout->addWidget(m_rememberCheck);

    m_errorLabel = new QLabel(card);
    m_errorLabel->setStyleSheet("color: #e53935; font-size: 12px; background: transparent;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_errorLabel);

    auto *loginBtn = new QPushButton(QStringLiteral("登 录"), card);
    loginBtn->setFixedHeight(38);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(
        "QPushButton { background: #2a5a9a; color: white; border: none;"
        "border-radius: 4px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #3a6aaa; }"
        "QPushButton:pressed { background: #1e4070; }");
    cardLayout->addWidget(loginBtn);

    mainLayout->addWidget(card);

    connect(loginBtn, &QPushButton::clicked, this, &LoginPage::onLoginClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginPage::onLoginClicked);

    // Load saved credentials
    if (m_settings.value("login/remember", false).toBool()) {
        m_rememberCheck->setChecked(true);
        m_usernameEdit->setText(m_settings.value("login/username").toString());
        m_passwordEdit->setText(m_settings.value("login/password").toString());
    }
}

void LoginPage::onLoginClicked()
{
    QString user = m_usernameEdit->text().trimmed();
    QString pass = m_passwordEdit->text();

    if (m_store->authenticate(user, pass)) {
        m_errorLabel->clear();
        if (m_rememberCheck->isChecked()) {
            m_settings.setValue("login/remember", true);
            m_settings.setValue("login/username", user);
            m_settings.setValue("login/password", pass);
        } else {
            m_settings.remove("login/remember");
            m_settings.remove("login/username");
            m_settings.remove("login/password");
        }
        emit loginSuccess();
    } else {
        m_errorLabel->setText(QStringLiteral("用户名或密码错误"));
    }
}
