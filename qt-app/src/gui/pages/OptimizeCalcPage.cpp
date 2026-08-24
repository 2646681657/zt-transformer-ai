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
#include <QSizePolicy>
#include <QToolButton>
#include <QMenu>
#include <QSettings>

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
    backBtn->setStyleSheet("QPushButton { color: #8a9bb0; font-size: 11px; border: none; padding: 2px 8px; }"
                           "QPushButton:hover { background: rgba(0,188,212,0.2); border-radius: 3px; color: #4dd0e1; }");
    connect(backBtn, &QPushButton::clicked, this, &OptimizeCalcPage::navigateBack);
    titleLayout->addWidget(backBtn);

    auto *titleLabel = new QLabel(
        QStringLiteral("中天伯乐达变压器电磁计算AI寻优软件 V2.0"), titleWidget);
    titleLabel->setObjectName("pageTitleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #e0e6ed; font-size: 12px;");
    titleLayout->addWidget(titleLabel, 1);

    mainLayout->addWidget(titleWidget);

    // Ribbon 头部栏：方案库下拉 + 设计输入静态标签
    auto *headerBar = new QWidget(this);
    headerBar->setFixedHeight(28);
    headerBar->setObjectName("RibbonHeaderBar");
    headerBar->setStyleSheet(
        "QWidget#RibbonHeaderBar { background: #2a2f38; border-bottom: 1px solid #3a4050; }");
    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(4);

    // 左控件：方案库下拉按钮
    auto *schemeBtn = new QToolButton(headerBar);
    schemeBtn->setText(QStringLiteral("方案库"));
    schemeBtn->setPopupMode(QToolButton::InstantPopup);
    schemeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    schemeBtn->setStyleSheet(
        "QToolButton { background: rgba(0,188,212,0.15); color: #4dd0e1;"
        "border: 1px solid #3a4050; border-radius: 4px; font-size: 11px; padding: 4px 16px; }"
        "QToolButton:hover { background: #00bcd4; color: #0d1117; border-color: #00bcd4; }"
        "QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: right center; width: 8px; }");
    auto *schemeMenu = new QMenu(schemeBtn);
    schemeMenu->addAction(QStringLiteral("方案一"));
    schemeMenu->addAction(QStringLiteral("方案二"));
    schemeMenu->addAction(QStringLiteral("方案三"));
    schemeBtn->setMenu(schemeMenu);
    connect(schemeMenu, &QMenu::triggered, headerBar, [schemeBtn](QAction *action) {
        schemeBtn->setText(action->text());
    });
    headerLayout->addWidget(schemeBtn);

    // 右控件：设计输入静态高亮按钮（禁用交互）
    auto *designInputBtn = new QPushButton(QStringLiteral("设计输入"), headerBar);
    designInputBtn->setEnabled(false);
    designInputBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "stop:0 #00bcd4, stop:1 #0097a7); color: #ffffff;"
        "border: none; font-size: 12px; font-weight: bold; padding: 4px 12px; }"
        "QPushButton:disabled { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "stop:0 #00bcd4, stop:1 #0097a7); color: #ffffff; }");
    headerLayout->addWidget(designInputBtn, 1);

    mainLayout->addWidget(headerBar);

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
    areaLayout->addWidget(m_navButton);
    areaLayout->addWidget(m_sidebar);
    areaLayout->addWidget(m_paramTable, 1);
    areaLayout->addWidget(m_helpPanel);
    mainLayout->addWidget(mainArea, 1);
}

void OptimizeCalcPage::setupRibbon()
{
    // Group 1: 计算模式 (互斥)：记住上次选择（默认正常模式）
    auto *g1 = m_ribbon->addGroup(QStringLiteral("计算模式"));
    g1->setExclusive(true);
    auto *normalBtn = new RibbonButton(QStringLiteral("正常模式"), ":/icons/mode_normal.svg", g1);
    g1->addButton(normalBtn);
    auto *proBtn = new RibbonButton(QStringLiteral("专业模式"), ":/icons/mode_pro.svg", g1);
    g1->addButton(proBtn);
    m_modeGroup = g1;
    {
        QSettings settings("ZTF", "Designer");
        const bool proMode = settings.value("optimize/proMode", false).toBool();
        (proMode ? proBtn : normalBtn)->setActive(true);
        m_config.calcMode = proMode ? StructureConfig::Professional : StructureConfig::Normal;
    }
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
    g3->addButton(new RibbonButton(QStringLiteral("长圆形"), ":/icons/shape_long_round.svg", g3));
    g3->addButton(new RibbonButton(QStringLiteral("椭圆形"), ":/icons/shape_ellipse.svg", g3));
    g3->addButton(new RibbonButton(QStringLiteral("半椭圆形"), ":/icons/shape_half_ellipse.svg", g3));
    auto *elBtn = new RibbonButton(QStringLiteral("类椭圆型"), ":/icons/shape_ellipse_like.svg", g3);
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
    connect(enterCalcBtn, &QToolButton::clicked, this, &OptimizeCalcPage::onEnterCalcClicked);
    g6->addButton(enterCalcBtn);
    auto *verifyBtn = new RibbonButton(QStringLiteral("校验算单"), ":/icons/verify.svg", g6);
    verifyBtn->setCheckable(false);
    g6->addButton(verifyBtn);

    // 保存选项分组引用用于验证
    m_selectGroups = {g1, g2, g3, g4, g5};

    // 连接选型变化信号到参数表刷新
    for (auto *group : m_selectGroups) {
        connect(group, &RibbonGroup::selectionChanged, this, &OptimizeCalcPage::onSelectionChanged);
    }
}

void OptimizeCalcPage::setupMainArea()
{
    // 竖排"程序选择"导航按钮（点击返回主界面），顶格放置于侧边栏左侧
    m_navButton = new QPushButton(QStringLiteral("程\n序\n选\n择"), this);
    m_navButton->setCursor(Qt::PointingHandCursor);
    m_navButton->setToolTip(QStringLiteral("返回主界面"));
    m_navButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_navButton->setStyleSheet(
        "QPushButton { background: rgba(0,188,212,0.15); color: #4dd0e1;"
        "border: none; border-right: 1px solid #3a4050; border-radius: 0px;"
        "font-size: 12px; padding: 4px; }"
        "QPushButton:hover { background: #00bcd4; color: #0d1117; }");
    connect(m_navButton, &QPushButton::clicked, this, &OptimizeCalcPage::navigateBack);

    // Left sidebar
    m_sidebar = new SidebarPanel(this);
    m_sidebar->addButton(QStringLiteral("选用推荐方案"), ":/icons/recommend.svg");
    m_sidebar->addButton(QStringLiteral("保存为我的方案"), ":/icons/save_scheme.svg");
    m_sidebar->addButton(QStringLiteral("从方案库中选择"), ":/icons/library.svg");
    m_sidebar->addButton(QStringLiteral("从记忆库中选择"), ":/icons/memory.svg");
    m_sidebar->addButton(QStringLiteral("采用上一次方案"), ":/icons/undo.svg");
    auto *enterBtn = m_sidebar->addButton(QStringLiteral("进入计算"), ":/icons/enter_calc.svg");
    connect(enterBtn, &QToolButton::clicked, this, &OptimizeCalcPage::onEnterCalcClicked);

    // Param table（设计变量初值取自 m_input，默认即 SB20-M-630-10；
    // 计算模式已在 setupRibbon 中从 QSettings 恢复）
    m_paramTable = new ParamTableWidget(this);
    m_paramTable->loadParamsForConfig(m_params, m_config, m_input,
                                      m_config.calcMode == StructureConfig::Professional);

    // Help panel
    m_helpPanel = new QTextEdit(this);
    m_helpPanel->setFixedWidth(200);
    m_helpPanel->setReadOnly(true);
    m_helpPanel->setPlainText(QStringLiteral(
        "操作说明:\n\n"
        "1. 在左侧选择设计方案\n"
        "2. 在中间表格修改参数\n"
        "3. 确认后点击\"进入计算\""));
    m_helpPanel->setStyleSheet("QTextEdit { background: #1e2228; border-left: 1px solid #3a4050;"
                               "color: #8a9bb0; }");
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
    m_params = m_paramTable->getParams();
    m_paramTable->saveToInput(m_input);   // 收集表格中编辑的设计变量
    updateConfigFromRibbon();
    emit navigateToEnterCalc();
}

// Ribbon选项变更时同步更新结构配置并刷新参数表
void OptimizeCalcPage::onSelectionChanged()
{
    updateConfigFromRibbon();
    refreshParamTable();
}

// 从Ribbon各分组的选中索引映射到StructureConfig枚举值
void OptimizeCalcPage::updateConfigFromRibbon()
{
    int idx;

    idx = m_selectGroups[0]->selectedIndex();
    m_config.calcMode = (idx == 1) ? StructureConfig::Professional : StructureConfig::Normal;

    idx = m_selectGroups[1]->selectedIndex();
    switch (idx) {
    case 0: m_config.coreType = StructureConfig::StackedSilicon; break;
    case 1: m_config.coreType = StructureConfig::StereoscopicRoll; break;
    case 2: m_config.coreType = StructureConfig::PlanarAmorphous; break;
    }

    idx = m_selectGroups[2]->selectedIndex();
    switch (idx) {
    case 0: m_config.coreShape = StructureConfig::Circle; break;
    case 1: m_config.coreShape = StructureConfig::LongRound; break;
    case 2: m_config.coreShape = StructureConfig::Ellipse; break;
    case 3: m_config.coreShape = StructureConfig::HalfEllipse; break;
    case 4: m_config.coreShape = StructureConfig::EllipseLike; break;
    }

    idx = m_selectGroups[3]->selectedIndex();
    m_config.windingForm = (idx == 1) ? StructureConfig::DualSplit : StructureConfig::Dual;

    idx = m_selectGroups[4]->selectedIndex();
    m_config.hvCoilStructure = (idx == 1) ? StructureConfig::TwoSegCylinder : StructureConfig::MultiLayerCylinder;
}

// 保存当前表格编辑内容（含设计变量）后按新配置重新加载参数表；
// 专业模式追加七~十节高级参数，两模式共享同一份 m_input（切回正常模式保留专业模式输入值）
void OptimizeCalcPage::refreshParamTable()
{
    m_params = m_paramTable->getParams();
    m_paramTable->saveToInput(m_input);   // 刷新前保留已编辑的设计变量
    const bool proMode = (m_config.calcMode == StructureConfig::Professional);
    m_paramTable->loadParamsForConfig(m_params, m_config, m_input, proMode);
    saveModePreference();
}

// 记住上次的计算模式选择（正常/专业）
void OptimizeCalcPage::saveModePreference() const
{
    QSettings settings("ZTF", "Designer");
    settings.setValue("optimize/proMode",
                      m_config.calcMode == StructureConfig::Professional);
}

// 外部设置结构配置（由类型选择对话框调用），更新标题栏并刷新参数表
void OptimizeCalcPage::setStructureConfig(const StructureConfig &config)
{
    m_config = config;

    // 更新标题栏显示当前选择的类型
    QString catStr = (config.category == StructureConfig::OilImmersed) ?
        QStringLiteral("油浸式变压器") : QStringLiteral("干式变压器");
    QString procStr = (config.windingProcess == StructureConfig::FoilWound) ?
        QStringLiteral("箔绕") : QStringLiteral("线绕");
    auto *titleLabel = findChild<QLabel *>("pageTitleLabel");
    if (titleLabel) {
        titleLabel->setText(catStr + " - " + procStr +
            QStringLiteral(" 电磁计算AI寻优 V2.0"));
    }

    refreshParamTable();
}