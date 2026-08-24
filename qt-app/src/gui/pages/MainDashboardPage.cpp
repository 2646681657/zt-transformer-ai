#include "MainDashboardPage.h"
#include "DataQueryPage.h"
#include "QuotePage.h"
#include "ImpedanceCalcPage.h"
#include "LossCalcPage.h"
#include "DataManagementPage.h"
#include "UserManagementPage.h"
#include "SettingsPage.h"
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
#include <QButtonGroup>
#include <QStackedWidget>

MainDashboardPage::MainDashboardPage(const QString &username, QWidget *parent)
    : QWidget(parent), m_username(username), m_navGroup(nullptr), m_subStack(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("MainDashboardPage { background: #1a1d23; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top navigation area (light blue background with icons)
    auto *navArea = new QWidget(this);
    navArea->setStyleSheet("background: #22262e; border-bottom: 1px solid #00bcd4;");
    auto *navAreaLayout = new QHBoxLayout(navArea);
    navAreaLayout->setContentsMargins(20, 12, 20, 12);
    navAreaLayout->setSpacing(0);
    setupToolBar(navAreaLayout);
    mainLayout->addWidget(navArea);

    // Sub buttons area (动态切换，根据选中的主按钮显示对应子按钮；
    // 选中"数据查询"时整体隐藏，让内容区直接显示数据查询界面)
    m_subArea = new QWidget(this);
    m_subArea->setStyleSheet("background: #1e2228; border-bottom: 1px solid #3a4050;");
    m_subArea->setFixedHeight(110);
    auto *subLayout = new QVBoxLayout(m_subArea);
    subLayout->setContentsMargins(0, 0, 0, 0);
    subLayout->setSpacing(0);
    m_subStack = new QStackedWidget(m_subArea);
    subLayout->addWidget(m_subStack);
    mainLayout->addWidget(m_subArea);
    setupSubArea();

    // Content area（空白页 + 内嵌功能页，点击"产品报价"/"数据查询"
    // 主按钮直接在内容区显示对应界面）
    m_contentStack = new QStackedWidget(this);
    m_contentStack->setStyleSheet("QStackedWidget { background: #1a1d23; }");
    m_contentStack->addWidget(new QWidget(m_contentStack));   // index 0: 空白
    m_quotePage = new QuotePage(this);
    m_contentStack->addWidget(m_quotePage);                   // index 1: 产品报价
    m_dataQueryPage = new DataQueryPage(this);
    m_contentStack->addWidget(m_dataQueryPage);               // index 2: 数据查询
    m_impedanceCalcPage = new ImpedanceCalcPage(this);
    m_contentStack->addWidget(m_impedanceCalcPage);           // index 3: 阻抗计算器
    m_lossCalcPage = new LossCalcPage(this);
    m_contentStack->addWidget(m_lossCalcPage);               // index 4: 损耗计算器
    m_dataMgmtPage = new DataManagementPage(this);
    m_contentStack->addWidget(m_dataMgmtPage);               // index 5: 数据管理
    m_userMgmtPage = new UserManagementPage(this);
    m_contentStack->addWidget(m_userMgmtPage);               // index 6: 用户管理
    m_settingsPage = new SettingsPage(this);
    m_contentStack->addWidget(m_settingsPage);               // index 7: 系统设置
    m_contentStack->setCurrentIndex(0);
    mainLayout->addWidget(m_contentStack, 1);

    // Footer
    auto *footer = new QWidget(this);
    footer->setFixedHeight(26);
    footer->setStyleSheet("background: #22262e; border-top: 1px solid #3a4050;");
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 0, 12, 0);
    int hour = QTime::currentTime().hour();
    QString greeting = hour < 12 ? "上午好" : (hour < 18 ? "下午好" : "晚上好");
    auto *footerLabel = new QLabel(
        QString("%1, %2! 欢迎进入程序选择！").arg(greeting, m_username), footer);
    footerLabel->setStyleSheet("font-size: 11px; color: #8a9bb0;");
    footerLayout->addWidget(footerLabel);
    footerLayout->addStretch();
    mainLayout->addWidget(footer);
}

void MainDashboardPage::setupToolBar(QHBoxLayout *layout)
{
    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    // 4 large navigation icons
    struct NavItem { QString text; QString icon; };
    QVector<NavItem> navItems = {
        {QStringLiteral("优化设计"), ":/icons/optimize.svg"},
        {QStringLiteral("产品报价"), ":/icons/quote.svg"},
        {QStringLiteral("程序工具"), ":/icons/tools.svg"},
        {QStringLiteral("数据查询"), ":/icons/search.svg"},
    };

    for (int i = 0; i < navItems.size(); ++i) {
        auto *btn = new QToolButton(this);
        btn->setText(navItems[i].text);
        btn->setIcon(QIcon(navItems[i].icon));
        btn->setIconSize(QSize(60, 60));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(112, 92);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);
        btn->setStyleSheet(
            "QToolButton { background: transparent; border: none; color: #e0e6ed; font-size: 13px; }"
            "QToolButton:hover { background: rgba(0,188,212,0.15); border-radius: 6px; color: #4dd0e1; }"
            "QToolButton:checked { background: rgba(0,188,212,0.2); border-radius: 6px; color: #4dd0e1; }");
        m_navGroup->addButton(btn, i);
        layout->addWidget(btn);
    }

    connect(m_navGroup, &QButtonGroup::idClicked, this, &MainDashboardPage::onNavButtonClicked);

    layout->addStretch();

    // Right-side tool buttons
    QStringList toolTexts = {"数据管理", "帮助", "退出"};
    QStringList toolIcons = {":/icons/database.svg", ":/icons/help.svg", ":/icons/logout.svg"};
    for (int i = 0; i < toolTexts.size(); ++i) {
        auto *btn = new QPushButton(QIcon(toolIcons[i]), toolTexts[i], this);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { color: #8a9bb0; font-size: 11px; padding: 4px 10px;"
            "border: 1px solid #3a4050; border-radius: 4px; background: #2a2f38; }"
            "QPushButton:hover { background: rgba(0,188,212,0.15); border-color: #00bcd4; color: #4dd0e1; }");
        if (toolTexts[i] == "退出")
            connect(btn, &QPushButton::clicked, this, &MainDashboardPage::onLogoutClicked);
        layout->addWidget(btn);
    }
}

void MainDashboardPage::setupSubArea()
{
    // index 0: 默认空白页（未点击任何主按钮时显示）
    m_subStack->addWidget(new QWidget(m_subStack));
    // index 1: 优化设计 - 子按钮（主按钮 0 点击后显示）
    m_subStack->addWidget(createOptimizeSubPage());
    // index 2: 程序工具 - 子按钮（阻抗计算器/损耗计算器）
    m_subStack->addWidget(createToolsSubPage());
    // index 3: 数据查询 - 空白页（主按钮直接在内容区显示，不走子按钮区）
    m_subStack->addWidget(new QWidget(m_subStack));
    // 默认显示空白页
    m_subStack->setCurrentIndex(0);
}

QWidget *MainDashboardPage::createOptimizeSubPage()
{
    // 子按钮页：图标在上文字在下，图标略小于上方主按钮，左边距缩进更多
    auto *page = new QWidget(m_subStack);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(40, 10, 20, 10);
    layout->setSpacing(24);

    // "优化设计" 子按钮
    auto *calcBtn = new QToolButton(page);
    calcBtn->setText(QStringLiteral("优化设计"));
    calcBtn->setIcon(QIcon(":/icons/calculate.svg"));
    calcBtn->setIconSize(QSize(48, 48));
    calcBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    calcBtn->setFixedSize(96, 84);
    calcBtn->setCursor(Qt::PointingHandCursor);
    calcBtn->setStyleSheet(
        "QToolButton { background: transparent; border: none; color: #c0c8d0; font-size: 12px; }"
        "QToolButton:hover { background: rgba(0,188,212,0.15); border-radius: 6px; color: #4dd0e1; }");
    connect(calcBtn, &QToolButton::clicked, this, &MainDashboardPage::navigateToOptimizeCalc);
    layout->addWidget(calcBtn);

    // "SW参数化出图" 子按钮（暂未实现）
    auto *swBtn = new QToolButton(page);
    swBtn->setText(QStringLiteral("SW参数化出图"));
    swBtn->setIcon(QIcon(":/icons/tools.svg"));
    swBtn->setIconSize(QSize(48, 48));
    swBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    swBtn->setFixedSize(96, 84);
    swBtn->setCursor(Qt::PointingHandCursor);
    swBtn->setStyleSheet(
        "QToolButton { background: transparent; border: none; color: #c0c8d0; font-size: 12px; }"
        "QToolButton:hover { background: rgba(0,188,212,0.15); border-radius: 6px; color: #4dd0e1; }");
    layout->addWidget(swBtn);

    layout->addStretch();
    return page;
}

QWidget *MainDashboardPage::createToolsSubPage()
{
    auto *page = new QWidget(m_subStack);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(40, 10, 20, 10);
    layout->setSpacing(24);

    auto makeToolBtn = [page](const QString &text, const QString &icon) {
        auto *btn = new QToolButton(page);
        btn->setText(text);
        btn->setIcon(QIcon(icon));
        btn->setIconSize(QSize(48, 48));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(96, 84);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QToolButton { background: transparent; border: none; color: #c0c8d0; font-size: 12px; }"
            "QToolButton:hover { background: rgba(0,188,212,0.15); border-radius: 6px; color: #4dd0e1; }");
        return btn;
    };

    auto *impBtn = makeToolBtn(QStringLiteral("阻抗计算器"), ":/icons/calculate.svg");
    connect(impBtn, &QToolButton::clicked, this, [this]() {
        m_contentStack->setCurrentWidget(m_impedanceCalcPage);
    });
    layout->addWidget(impBtn);

    auto *lossBtn = makeToolBtn(QStringLiteral("损耗计算器"), ":/icons/calculate.svg");
    connect(lossBtn, &QToolButton::clicked, this, [this]() {
        m_contentStack->setCurrentWidget(m_lossCalcPage);
    });
    layout->addWidget(lossBtn);

    auto *mgmtBtn = makeToolBtn(QStringLiteral("数据管理"), ":/icons/tools.svg");
    connect(mgmtBtn, &QToolButton::clicked, this, [this]() {
        m_contentStack->setCurrentWidget(m_dataMgmtPage);
    });
    layout->addWidget(mgmtBtn);

    auto *userBtn = makeToolBtn(QStringLiteral("用户管理"), ":/icons/help.svg");
    connect(userBtn, &QToolButton::clicked, this, [this]() {
        m_contentStack->setCurrentWidget(m_userMgmtPage);
    });
    layout->addWidget(userBtn);

    auto *settingsBtn = makeToolBtn(QStringLiteral("系统设置"), ":/icons/tools.svg");
    connect(settingsBtn, &QToolButton::clicked, this, [this]() {
        m_contentStack->setCurrentWidget(m_settingsPage);
    });
    layout->addWidget(settingsBtn);

    layout->addStretch();
    return page;
}

void MainDashboardPage::loadQuoteScheme(const TransformerParams &params, const CalcInput &input,
                                        const CalcResult &result)
{
    m_quotePage->loadScheme(params, input, result);
}

void MainDashboardPage::onNavButtonClicked(int index)
{
    // 数据查询（主按钮 3）：隐藏子按钮区，内容区直接显示数据查询界面
    if (index == 3) {
        m_subArea->hide();
        m_contentStack->setCurrentWidget(m_dataQueryPage);
        return;
    }
    // 产品报价（主按钮 1）：同样隐藏子按钮区，内容区直接显示报价界面
    if (index == 1) {
        m_subArea->hide();
        m_contentStack->setCurrentWidget(m_quotePage);
        return;
    }
    // 其他主按钮：恢复子按钮区，内容区回到空白页
    m_subArea->show();
    m_contentStack->setCurrentIndex(0);
    // 子按钮区索引映射：
    //   主按钮 0（优化设计）→ subStack 1
    //   主按钮 2（程序工具）→ subStack 2
    if (index == 0)
        m_subStack->setCurrentIndex(1);
    else if (index == 2)
        m_subStack->setCurrentIndex(2);
    else
        m_subStack->setCurrentIndex(0);
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