#include "OptimizeCalcPage.h"
#include "RibbonBar.h"
#include "RibbonGroup.h"
#include "RibbonButton.h"
#include "ParamTableWidget.h"
#include "SidebarPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>

OptimizeCalcPage::OptimizeCalcPage(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar
    auto *titleBar = new QLabel(
        QStringLiteral("油浸式变压器电磁计算(长方形线圈的非晶合金铁心)导线优化设计软件 V2.0-2025"), this);
    titleBar->setFixedHeight(28);
    titleBar->setAlignment(Qt::AlignCenter);
    titleBar->setObjectName("PageTitleBar");
    mainLayout->addWidget(titleBar);

    // Ribbon
    m_ribbon = new RibbonBar(this);
    setupRibbon();
    mainLayout->addWidget(m_ribbon);

    // Main area
    auto *mainArea = new QWidget(this);
    auto *areaLayout = new QHBoxLayout(mainArea);
    areaLayout->setContentsMargins(0, 0, 0, 0);
    areaLayout->setSpacing(0);
    setupMainArea();
    areaLayout->addWidget(m_sidebar);
    areaLayout->addWidget(m_paramTable, 1);
    areaLayout->addWidget(m_helpPanel);
    mainLayout->addWidget(mainArea, 1);
}

void OptimizeCalcPage::setupRibbon()
{
    // Group 1: 计算模式
    auto *g1 = m_ribbon->addGroup(QStringLiteral("计算模式"));
    auto *normalBtn = new RibbonButton(QStringLiteral("正常模式"), g1);
    normalBtn->setActive(true);
    g1->addButton(normalBtn);
    g1->addButton(new RibbonButton(QStringLiteral("专业模式"), g1));
    m_ribbon->addSeparator();

    // Group 2: 变压器结构
    auto *g2 = m_ribbon->addGroup(QStringLiteral("变压器结构"));
    g2->addButton(new RibbonButton(QStringLiteral("叠铁芯"), g2));
    g2->addButton(new RibbonButton(QStringLiteral("立体卷铁芯"), g2));
    auto *amBtn = new RibbonButton(QStringLiteral("平面非晶合金"), g2);
    amBtn->setActive(true);
    g2->addButton(amBtn);
    m_ribbon->addSeparator();

    // Group 3: 铁芯结构
    auto *g3 = m_ribbon->addGroup(QStringLiteral("铁芯结构"));
    g3->addButton(new RibbonButton(QStringLiteral("圆形"), g3));
    g3->addButton(new RibbonButton(QStringLiteral("长圆形"), g3));
    g3->addButton(new RibbonButton(QStringLiteral("椭圆形"), g3));
    g3->addButton(new RibbonButton(QStringLiteral("半椭圆形"), g3));
    auto *elBtn = new RibbonButton(QStringLiteral("类椭圆型"), g3);
    elBtn->setActive(true);
    g3->addButton(elBtn);
    m_ribbon->addSeparator();

    // Group 4: 绕组方式
    auto *g4 = m_ribbon->addGroup(QStringLiteral("绕组方式"));
    auto *dualBtn = new RibbonButton(QStringLiteral("双绕组"), g4);
    dualBtn->setActive(true);
    g4->addButton(dualBtn);
    g4->addButton(new RibbonButton(QStringLiteral("双分裂"), g4));
    m_ribbon->addSeparator();

    // Group 5: 高压线圈结构
    auto *g5 = m_ribbon->addGroup(QStringLiteral("高压线圈结构"));
    auto *multiBtn = new RibbonButton(QStringLiteral("多层圆筒式"), g5);
    multiBtn->setActive(true);
    g5->addButton(multiBtn);
    g5->addButton(new RibbonButton(QStringLiteral("两段圆筒式"), g5));
    m_ribbon->addSeparator();

    // Group 6: 确认设置
    auto *g6 = m_ribbon->addGroup(QStringLiteral("确认设置"));
    auto *enterCalcBtn = new RibbonButton(QStringLiteral("进入计算"), g6);
    connect(enterCalcBtn, &QPushButton::clicked, this, &OptimizeCalcPage::navigateToEnterCalc);
    g6->addButton(enterCalcBtn);
    g6->addButton(new RibbonButton(QStringLiteral("校验算单"), g6));
}

void OptimizeCalcPage::setupMainArea()
{
    // Left sidebar
    m_sidebar = new SidebarPanel(this);
    m_sidebar->addButton(QStringLiteral("选用推荐方案"));
    m_sidebar->addButton(QStringLiteral("保存为我的方案"));
    m_sidebar->addButton(QStringLiteral("从方案库中选择"));
    m_sidebar->addButton(QStringLiteral("从记忆库中选择"));
    m_sidebar->addButton(QStringLiteral("采用上一次方案"));
    auto *enterBtn = m_sidebar->addButton(QStringLiteral("进入计算"));
    connect(enterBtn, &QPushButton::clicked, this, &OptimizeCalcPage::navigateToEnterCalc);

    // Param table
    m_paramTable = new ParamTableWidget(this);
    m_paramTable->loadParams(m_params);

    // Help panel
    m_helpPanel = new QTextEdit(this);
    m_helpPanel->setFixedWidth(200);
    m_helpPanel->setReadOnly(true);
    m_helpPanel->setPlainText(QStringLiteral(
        "操作说明:\n\n"
        "1. 在左侧选择设计方案\n"
        "2. 在中间表格修改参数\n"
        "3. 确认后点击\"进入计算\""));
    m_helpPanel->setStyleSheet("QTextEdit { background: #f8f8f8; border-left: 1px solid #ddd; }");
}