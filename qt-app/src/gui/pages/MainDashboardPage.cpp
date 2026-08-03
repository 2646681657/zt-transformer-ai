#include "MainDashboardPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTime>
#include <QIcon>
#include <QFrame>
#include <QEvent>
#include <QMouseEvent>
#include <QToolButton>

MainDashboardPage::MainDashboardPage(const QString &username, QWidget *parent)
    : QWidget(parent), m_username(username)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("MainDashboardPage { background: #d6e4f0; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top navigation area (light blue background with icons)
    auto *navArea = new QWidget(this);
    navArea->setStyleSheet("background: #dce8f4; border-bottom: 2px solid #7eaed4;");
    auto *navAreaLayout = new QHBoxLayout(navArea);
    navAreaLayout->setContentsMargins(20, 12, 20, 12);
    navAreaLayout->setSpacing(0);
    setupToolBar(navAreaLayout);
    mainLayout->addWidget(navArea);

    // Sub buttons area (action shortcuts)
    auto *subArea = new QWidget(this);
    subArea->setStyleSheet("background: #eaf2fa; border-bottom: 1px solid #c0d4e8;");
    subArea->setFixedHeight(90);
    auto *subLayout = new QHBoxLayout(subArea);
    subLayout->setContentsMargins(20, 10, 20, 10);
    subLayout->setSpacing(20);
    setupCardArea(subLayout);
    mainLayout->addWidget(subArea);

    // Content area
    auto *content = new QWidget(this);
    content->setStyleSheet("background: #d6e4f0;");
    mainLayout->addWidget(content, 1);

    // Footer
    auto *footer = new QWidget(this);
    footer->setFixedHeight(26);
    footer->setStyleSheet("background: #c8dced; border-top: 1px solid #a0bcd8;");
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 0, 12, 0);
    int hour = QTime::currentTime().hour();
    QString greeting = hour < 12 ? "上午好" : (hour < 18 ? "下午好" : "晚上好");
    auto *footerLabel = new QLabel(
        QString("%1, %2! 欢迎进入程序选择！").arg(greeting, m_username), footer);
    footerLabel->setStyleSheet("font-size: 11px; color: #336;");
    footerLayout->addWidget(footerLabel);
    footerLayout->addStretch();
    mainLayout->addWidget(footer);
}

void MainDashboardPage::setupToolBar(QHBoxLayout *layout)
{
    // 4 large navigation icons
    struct NavItem { QString text; QString icon; };
    QVector<NavItem> navItems = {
        {"优化设计", ":/icons/optimize_dark.svg"},
        {"产品报价", ":/icons/quote_dark.svg"},
        {"程序工具", ":/icons/tools_dark.svg"},
        {"数据查询", ":/icons/search_dark.svg"},
    };

    for (int i = 0; i < navItems.size(); ++i) {
        auto *btn = new QToolButton(this);
        btn->setText(navItems[i].text);
        btn->setIcon(QIcon(navItems[i].icon));
        btn->setIconSize(QSize(40, 40));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(80, 70);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QToolButton { background: transparent; border: none; color: #2a5a9a; font-size: 12px; }"
            "QToolButton:hover { background: rgba(42,90,154,0.1); border-radius: 6px; }");
        layout->addWidget(btn);
    }

    layout->addStretch();

    // Right-side tool buttons
    QStringList toolTexts = {"数据管理", "帮助", "退出"};
    QStringList toolIcons = {":/icons/database_dark.svg", ":/icons/help_dark.svg", ":/icons/logout_dark.svg"};
    for (int i = 0; i < toolTexts.size(); ++i) {
        auto *btn = new QPushButton(QIcon(toolIcons[i]), toolTexts[i], this);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { color: #444; font-size: 11px; padding: 4px 8px; border: 1px solid #bbb; border-radius: 3px; background: #f8f8f8; }"
            "QPushButton:hover { background: #e8e8e8; }");
        if (toolTexts[i] == "退出")
            connect(btn, &QPushButton::clicked, this, &MainDashboardPage::onLogoutClicked);
        layout->addWidget(btn);
    }
}

void MainDashboardPage::setupCardArea(QHBoxLayout *layout)
{
    // "优化计算" action button with icon
    auto *calcBtn = new QToolButton(this);
    calcBtn->setText(QStringLiteral("优化计算"));
    calcBtn->setIcon(QIcon(":/icons/calculate.svg"));
    calcBtn->setIconSize(QSize(28, 28));
    calcBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    calcBtn->setFixedHeight(36);
    calcBtn->setCursor(Qt::PointingHandCursor);
    calcBtn->setStyleSheet(
        "QToolButton { background: #2a5a9a; color: white; border-radius: 4px; padding: 4px 16px; font-size: 12px; }"
        "QToolButton:hover { background: #3a6aaa; }");
    connect(calcBtn, &QToolButton::clicked, this, &MainDashboardPage::navigateToOptimizeCalc);
    layout->addWidget(calcBtn);

    // "SW参数化出图" (disabled)
    auto *swBtn = new QToolButton(this);
    swBtn->setText(QStringLiteral("SW参数化出图"));
    swBtn->setIcon(QIcon(":/icons/tools_dark.svg"));
    swBtn->setIconSize(QSize(28, 28));
    swBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    swBtn->setFixedHeight(36);
    swBtn->setEnabled(false);
    swBtn->setStyleSheet(
        "QToolButton { background: #ccc; color: #666; border-radius: 4px; padding: 4px 16px; font-size: 12px; }");
    layout->addWidget(swBtn);

    layout->addStretch();
}

void MainDashboardPage::onLogoutClicked()
{
    auto ret = QMessageBox::question(this, QStringLiteral("确认退出"),
        QStringLiteral("确定要退出登录吗？"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes)
        emit logoutRequested();
}

bool MainDashboardPage::eventFilter(QObject *obj, QEvent *event)
{
    return QWidget::eventFilter(obj, event);
}