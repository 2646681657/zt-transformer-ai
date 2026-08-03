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
#include <QPushButton>
#include <QMessageBox>

OptimizeCalcPage::OptimizeCalcPage(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar with back button
    auto *titleWidget = new QWidget(this);
    titleWidget->setFixedHeight(28);
    titleWidget->setObjectName("PageTitleBar");
    auto *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(4, 0, 4, 0);
    titleLayout->setSpacing(8);

    auto *backBtn = new QPushButton(QStringLiteral("< 返回"), titleWidget);
    backBtn->setFlat(true);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet("QPushButton { color: white; font-size: 11px; border: none; padding: 2px 8px; }"
                           "QPushButton:hover { background: rgba(255,255,255,0.2); border-radius: 3px; }");
    connect(backBtn, &QPushButton::clicked, this, &OptimizeCalcPage::navigateBack);
    titleLayout->addWidget(backBtn);

    auto *titleLabel = new QLabel(
        QStringLiteral("油浸式变压器电磁计算(长方形线圈的非晶合金铁心)导线优化设计软件 V2.0-2025"), titleWidget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: white; font-size: 12px;");
    titleLayout->addWidget(titleLabel, 1);

    mainLayout->addWidget(titleWidget);

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
    // Group 1: 计算模式 (互斥)
    auto *g1 = m_ribbon->addGroup(QStringLiteral("计算模式"));
    g1->setExclusive(true);
    auto *normalBtn = new RibbonButton(QStringLiteral("正常模式"), ":/icons/mode_normal.svg", g1);
    normalBtn->setActive(true);
    g1->addButton(normalBtn);
    g1->addButton(new RibbonButton(QStringLiteral("专业模式"), ":/icons/mode_pro.svg", g1));
    m_ribbon->addSeparator();

    // Group 2: 变压器结构 (互斥)
    auto *g2 = m_ribbon->addGroup(QStringLiteral("变压器结构"));
    g2->setExclusive(true);
    g2->addButton(new RibbonButton(QStringLiteral("叠铁芯"), ":/icons/core_stack.svg", g2));
    g2->addButton(new RibbonButton(QStringLiteral("立体卷铁芯"), ":/icons/core_roll.svg", g2));
    auto *amBtn = new RibbonButton(QStringLiteral("平面非晶合金"), ":/icons/core_amorphous.svg", g2);
    amBtn->setActive(true);
    g2->addButton(amBtn);
    m_ribbon->addSeparator();

    // Group 3: 铁芯结构 (互斥)
    auto *g3 = m_ribbon->addGroup(QStringLiteral("铁芯结构"));
    g3->setExclusive(true);
    g3->addButton(new RibbonButton(QStringLiteral("圆形"), ":/icons/shape_circle.svg", g3));
    g3->addButton(new RibbonButton(QStringLiteral("长圆形"), ":/icons/shape_circle.svg", g3));
    g3->addButton(new RibbonButton(QStringLiteral("椭圆形"), ":/icons/shape_circle.svg", g3));
    g3->addButton(new RibbonButton(QStringLiteral("半椭圆形"), ":/icons/shape_circle.svg", g3));
    auto *elBtn = new RibbonButton(QStringLiteral("类椭圆型"), ":/icons/shape_circle.svg", g3);
    elBtn->setActive(true);
    g3->addButton(elBtn);
    m_ribbon->addSeparator();

    // Group 4: 绕组方式 (互斥)
    auto *g4 = m_ribbon->addGroup(QStringLiteral("绕组方式"));
    g4->setExclusive(true);
    auto *dualBtn = new RibbonButton(QStringLiteral("双绕组"), ":/icons/winding_dual.svg", g4);
    dualBtn->setActive(true);
    g4->addButton(dualBtn);
    g4->addButton(new RibbonButton(QStringLiteral("双分裂"), ":/icons/winding_split.svg", g4));
    m_ribbon->addSeparator();

    // Group 5: 高压线圈结构 (互斥)
    auto *g5 = m_ribbon->addGroup(QStringLiteral("高压线圈结构"));
    g5->setExclusive(true);
    auto *multiBtn = new RibbonButton(QStringLiteral("多层圆筒式"), ":/icons/coil_multi.svg", g5);
    multiBtn->setActive(true);
    g5->addButton(multiBtn);
    g5->addButton(new RibbonButton(QStringLiteral("两段圆筒式"), ":/icons/coil_two.svg", g5));
    m_ribbon->addSeparator();

    // Group 6: 确认设置 (非互斥，操作按钮)
    auto *g6 = m_ribbon->addGroup(QStringLiteral("确认设置"));
    auto *enterCalcBtn = new RibbonButton(QStringLiteral("进入计算"), ":/icons/enter_calc.svg", g6);
    enterCalcBtn->setCheckable(false);
    connect(enterCalcBtn, &QPushButton::clicked, this, &OptimizeCalcPage::onEnterCalcClicked);
    g6->addButton(enterCalcBtn);
    auto *verifyBtn = new RibbonButton(QStringLiteral("校验算单"), ":/icons/verify.svg", g6);
    verifyBtn->setCheckable(false);
    g6->addButton(verifyBtn);

    // 保存选项分组引用用于验证
    m_selectGroups = {g1, g2, g3, g4, g5};
}

void OptimizeCalcPage::setupMainArea()
{
    // Left sidebar
    m_sidebar = new SidebarPanel(this);
    m_sidebar->addButton(QStringLiteral("选用推荐方案"), ":/icons/recommend.svg");
    m_sidebar->addButton(QStringLiteral("保存为我的方案"), ":/icons/save_scheme.svg");
    m_sidebar->addButton(QStringLiteral("从方案库中选择"), ":/icons/library.svg");
    m_sidebar->addButton(QStringLiteral("从记忆库中选择"), ":/icons/memory.svg");
    m_sidebar->addButton(QStringLiteral("采用上一次方案"), ":/icons/undo.svg");
    auto *enterBtn = m_sidebar->addButton(QStringLiteral("进入计算"), ":/icons/enter_calc.svg");
    connect(enterBtn, &QPushButton::clicked, this, &OptimizeCalcPage::onEnterCalcClicked);

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

void OptimizeCalcPage::onEnterCalcClicked()
{
    QStringList groupNames = {"计算模式", "变压器结构", "铁芯结构", "绕组方式", "高压线圈结构"};
    for (int i = 0; i < m_selectGroups.size(); ++i) {
        if (!m_selectGroups[i]->hasSelection()) {
            QMessageBox::warning(this, QStringLiteral("选型未完成"),
                QString("请先选择「%1」选项").arg(groupNames[i]));
            return;
        }
    }
    emit navigateToEnterCalc();
}