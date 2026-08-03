#include "MainDashboardPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTime>

MainDashboardPage::MainDashboardPage(const QString &username, QWidget *parent)
    : QWidget(parent), m_username(username)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header
    auto *header = new QWidget(this);
    header->setFixedHeight(50);
    header->setStyleSheet("background: #2a5a9a;");
    auto *headerLayout = new QHBoxLayout(header);
    auto *titleLabel = new QLabel(QStringLiteral("同优计算优化设计软件(V2.0)"), header);
    titleLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    mainLayout->addWidget(header);

    // Nav bar
    auto *navBar = new QWidget(this);
    navBar->setFixedHeight(40);
    navBar->setStyleSheet("background: #3a6aaa;");
    auto *navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(10, 0, 10, 0);
    setupNavBar(navLayout);
    mainLayout->addWidget(navBar);

    // Sub buttons
    auto *subBar = new QWidget(this);
    subBar->setFixedHeight(36);
    subBar->setStyleSheet("background: #e8e8e8; border-bottom: 1px solid #ccc;");
    auto *subLayout = new QHBoxLayout(subBar);
    subLayout->setContentsMargins(10, 0, 10, 0);
    setupSubButtons(subLayout);
    mainLayout->addWidget(subBar);

    // Content area
    auto *content = new QWidget(this);
    content->setStyleSheet("background: #f8f8f8;");
    mainLayout->addWidget(content, 1);

    // Footer
    auto *footer = new QWidget(this);
    footer->setFixedHeight(28);
    footer->setStyleSheet("background: #e0e0e0; border-top: 1px solid #ccc;");
    auto *footerLayout = new QHBoxLayout(footer);
    int hour = QTime::currentTime().hour();
    QString greeting = hour < 12 ? "上午好" : (hour < 18 ? "下午好" : "晚上好");
    auto *footerLabel = new QLabel(
        QString("%1, %2! 欢迎进入程序选择！").arg(greeting, m_username), footer);
    footerLabel->setStyleSheet("font-size: 11px; color: #555;");
    footerLayout->addWidget(footerLabel);
    mainLayout->addWidget(footer);
}

void MainDashboardPage::setupNavBar(QLayout *parent)
{
    auto *layout = qobject_cast<QHBoxLayout *>(parent);
    QStringList navItems = {"优化设计", "产品报价", "程序工具", "数据查询"};
    for (const auto &item : navItems) {
        auto *btn = new QPushButton(item, this);
        btn->setFlat(true);
        btn->setStyleSheet(
            "QPushButton { color: white; font-size: 13px; padding: 4px 16px; border: none; }"
            "QPushButton:hover { background: rgba(255,255,255,0.2); border-radius: 3px; }");
        layout->addWidget(btn);
    }
    layout->addStretch();

    QStringList toolItems = {"数据管理", "帮助", "退出"};
    for (const auto &item : toolItems) {
        auto *btn = new QPushButton(item, this);
        btn->setFlat(true);
        btn->setStyleSheet(
            "QPushButton { color: #d0e0f0; font-size: 11px; padding: 4px 10px; border: none; }"
            "QPushButton:hover { color: white; }");
        if (item == "退出") {
            connect(btn, &QPushButton::clicked, this, &MainDashboardPage::onLogoutClicked);
        }
        layout->addWidget(btn);
    }
}

void MainDashboardPage::setupSubButtons(QLayout *parent)
{
    auto *layout = qobject_cast<QHBoxLayout *>(parent);
    auto *calcBtn = new QPushButton(QStringLiteral("优化计算"), this);
    calcBtn->setStyleSheet(
        "QPushButton { background: #2a5a9a; color: white; border-radius: 3px; padding: 4px 14px; }"
        "QPushButton:hover { background: #3a6aaa; }");
    connect(calcBtn, &QPushButton::clicked, this, &MainDashboardPage::navigateToOptimizeCalc);
    layout->addWidget(calcBtn);

    auto *swBtn = new QPushButton(QStringLiteral("SW参数化出图"), this);
    swBtn->setStyleSheet("QPushButton { padding: 4px 14px; }");
    swBtn->setEnabled(false);
    layout->addWidget(swBtn);
    layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));
}

void MainDashboardPage::onLogoutClicked()
{
    auto ret = QMessageBox::question(this, QStringLiteral("确认退出"),
        QStringLiteral("确定要退出登录吗？"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes)
        emit logoutRequested();
}
