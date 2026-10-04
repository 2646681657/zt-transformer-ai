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
                  "stop:0 #FFFFFF, stop:0.5 #F3F5F4, stop:1 #FFFFFF); }");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    auto *card = new QFrame(this);
    card->setFixedSize(400, 340);
    card->setStyleSheet("QFrame { background: #FFFFFF; border-radius: 12px;"
                        "border: 1px solid #D3DDD6; }");

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(14);
    cardLayout->setContentsMargins(36, 28, 36, 28);

    auto *title = new QLabel(QStringLiteral("中天伯乐达变压器电磁计算AI寻优软件"), card);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color: #185C37; font-size: 17px; font-weight: bold;"
                         "background: transparent;");
    cardLayout->addWidget(title);
    cardLayout->addSpacing(8);

    m_usernameEdit = new QLineEdit(card);
    m_usernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_usernameEdit->setFixedHeight(34);
    m_usernameEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #D3DDD6; border-radius: 6px; padding: 6px 12px;"
        "font-size: 13px; background: #F3F5F4; color: #24362B; }"
        "QLineEdit:focus { border: 1px solid #217346; background: #F7FAF8; }");
    cardLayout->addWidget(m_usernameEdit);

    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setFixedHeight(34);
    m_passwordEdit->setStyleSheet(m_usernameEdit->styleSheet());
    cardLayout->addWidget(m_passwordEdit);

    m_rememberCheck = new QCheckBox(QStringLiteral("记住账号密码"), card);
    m_rememberCheck->setStyleSheet("QCheckBox { color: #607368; font-size: 12px;"
                                   "background: transparent; }");
    cardLayout->addWidget(m_rememberCheck);

    m_errorLabel = new QLabel(card);
    m_errorLabel->setStyleSheet("color: #C53030; font-size: 12px; background: transparent;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_errorLabel);

    auto *loginBtn = new QPushButton(QStringLiteral("登 录"), card);
    loginBtn->setFixedHeight(38);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(
        "QPushButton { background: #217346; color: #FFFFFF; border: none;"
        "border-radius: 6px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #185C37; }"
        "QPushButton:pressed { background: #185C37; color: white; }");
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
