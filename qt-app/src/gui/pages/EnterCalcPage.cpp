#include "EnterCalcPage.h"
#include "RibbonBar.h"
#include "RibbonGroup.h"
#include "RibbonButton.h"
#include "SidebarPanel.h"
#include "SchemeTableWidget.h"
#include "PrintTableWidget.h"
#include "EmResultPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QSizePolicy>
#include <QToolButton>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QInputDialog>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QMenu>
#include <QPrinter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPainter>

EnterCalcPage::EnterCalcPage(QWidget *parent)
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
    connect(backBtn, &QPushButton::clicked, this, &EnterCalcPage::navigateBack);
    titleLayout->addWidget(backBtn);

    auto *titleLabel = new QLabel(
        QStringLiteral("中天伯乐达变压器电磁计算AI寻优软件 V2.0"), titleWidget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #e0e6ed; font-size: 12px;");
    titleLayout->addWidget(titleLabel, 1);

    mainLayout->addWidget(titleWidget);

    // Tab bar（位于 Ribbon 上方）
    m_tabBar = new QTabBar(this);
    m_tabBar->addTab(QStringLiteral("优化计算"));
    m_tabBar->addTab(QStringLiteral("方案选择"));
    m_tabBar->addTab(QStringLiteral("输出打印"));
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setFixedHeight(30);
    m_tabBar->setStyleSheet(
        "QTabBar { background: #22262e; border-bottom: 1px solid #3a4050; }"
        "QTabBar::pane { border: none; }"
        "QTabBar::tab { background: #2a2f38; color: #8a9bb0; border: 1px solid #3a4050;"
        "border-bottom: none; padding: 6px 18px; margin-right: 2px; font-size: 12px; }"
        "QTabBar::tab:selected { background: #0d1117; color: #4dd0e1;"
        "border-color: #00bcd4; border-bottom: 2px solid #00bcd4; }"
        "QTabBar::tab:hover:!selected { background: #3a4050; color: #e0e6ed; }");
    mainLayout->addWidget(m_tabBar);

    // Ribbon stack (shows different ribbon per tab)
    m_ribbonStack = new QWidget(this);
    auto *ribbonStackLayout = new QVBoxLayout(m_ribbonStack);
    ribbonStackLayout->setContentsMargins(0, 0, 0, 0);
    m_optimizeRibbon = new RibbonBar(m_ribbonStack);
    m_schemeRibbon = new RibbonBar(m_ribbonStack);
    m_printRibbon = new RibbonBar(m_ribbonStack);
    buildOptimizeRibbon();
    buildSchemeRibbon();
    buildPrintRibbon();
    ribbonStackLayout->addWidget(m_optimizeRibbon);
    ribbonStackLayout->addWidget(m_schemeRibbon);
    ribbonStackLayout->addWidget(m_printRibbon);
    m_schemeRibbon->hide();
    m_printRibbon->hide();
    mainLayout->addWidget(m_ribbonStack);

    // Stacked content
    m_stack = new QStackedWidget(this);
    setupOptimizeTab();
    setupSchemeTab();
    setupPrintTab();
    connect(m_tabBar, &QTabBar::currentChanged, this, &EnterCalcPage::onTabChanged);
    mainLayout->addWidget(m_stack, 1);

    // Status bar
    m_statusBar = new QLabel(QStringLiteral("就绪"), this);
    m_statusBar->setFixedHeight(22);
    m_statusBar->setStyleSheet("background: #22262e; padding-left: 8px; font-size: 11px;"
                               "color: #8a9bb0; border-top: 1px solid #3a4050;");
    mainLayout->addWidget(m_statusBar);
}

void EnterCalcPage::buildOptimizeRibbon()
{
    auto *g0 = m_optimizeRibbon->addGroup(QString());
    auto *quickBtn = new RibbonButton(QStringLiteral("快速计算"), ":/icons/fast_calc.svg", g0);
    quickBtn->setCheckable(false);
    connect(quickBtn, &QToolButton::clicked, this, &EnterCalcPage::onRunEmCalc);
    g0->addButton(quickBtn);
    auto *verifyBtn = new RibbonButton(QStringLiteral("对拍自检"), ":/icons/verify.svg", g0);
    verifyBtn->setCheckable(false);
    connect(verifyBtn, &QToolButton::clicked, this, &EnterCalcPage::onSelfTest);
    g0->addButton(verifyBtn);
    m_optimizeRibbon->addSeparator();

    auto *g1 = m_optimizeRibbon->addGroup(QStringLiteral("初始化设置(从左到右顺序设置)"));
    g1->addButton(new RibbonButton(QStringLiteral("产品结构"), ":/icons/product.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("材料选择"), ":/icons/material.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("修正参数"), ":/icons/adjust.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("循环参数"), ":/icons/loop_param.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("约束条件"), ":/icons/constraint.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("初始化信息"), ":/icons/init_info.svg", g1));
    m_optimizeRibbon->addSeparator();

    auto *gMem = m_optimizeRibbon->addGroup(QStringLiteral("学习记忆"));
    auto *memBtn = new RibbonButton(QStringLiteral("打开记忆功能"), ":/icons/memory_on.svg", gMem);
    memBtn->setCheckable(false);
    gMem->addButton(memBtn);
    m_optimizeRibbon->addSeparator();

    auto *g2 = m_optimizeRibbon->addGroup(QStringLiteral("寻优计算"));
    auto *startBtn = new RibbonButton(QStringLiteral("开始运行计算"), ":/icons/play.svg", g2);
    startBtn->setCheckable(false);
    connect(startBtn, &QToolButton::clicked, this, &EnterCalcPage::onRunEmCalc);
    g2->addButton(startBtn);
    auto *pauseBtn = new RibbonButton(QStringLiteral("暂停计算"), ":/icons/pause.svg", g2);
    pauseBtn->setCheckable(false);
    g2->addButton(pauseBtn);
    auto *stopBtn = new RibbonButton(QStringLiteral("停止计算"), ":/icons/stop.svg", g2);
    stopBtn->setCheckable(false);
    g2->addButton(stopBtn);
    m_optimizeRibbon->addSeparator();

    auto *g3 = m_optimizeRibbon->addGroup(QStringLiteral("方案库"));
    auto *schemeBtn = new RibbonButton(QStringLiteral("进入方案选择"), ":/icons/scheme.svg", g3);
    schemeBtn->setCheckable(false);
    connect(schemeBtn, &QToolButton::clicked, this, [this]() {
        m_tabBar->setCurrentIndex(1);
    });
    g3->addButton(schemeBtn);
}

// 方案选择 Ribbon：与原型图五组布局一致
// 显示选项 | 排序与筛选 | 合并模式 | 方案库比较 | 方案选择
void EnterCalcPage::buildSchemeRibbon()
{
    const QString chkStyle = QStringLiteral("QCheckBox { color: #e0e6ed; font-size: 11px; spacing: 4px; }");

    // ---- 组1 显示选项：两行（勾选+标签+控件）----
    auto *g1 = m_schemeRibbon->addGroup(QStringLiteral("显示选项"));
    auto *rows = new QWidget(g1);
    auto *rowLayout = new QVBoxLayout(rows);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(4);
    m_showMainChk = new QCheckBox(QStringLiteral("显示主要参数"), rows);
    m_showMainChk->setStyleSheet(chkStyle);
    connect(m_showMainChk, &QCheckBox::toggled, this, [this](bool on) {
        // 勾选=仅显示主要参数列，其余隐藏
        const QList<int> mainCols = {1, 2, 4, 6, 7, 8, 9, 10, 11};
        for (int c = 1; c < m_schemeTable->columnCount(); ++c)
            m_schemeTable->setColumnHidden(c, on && !mainCols.contains(c));
        m_statusBar->setText(on ? QStringLiteral("已切换为主要参数显示")
                                : QStringLiteral("已显示全部参数列"));
    });
    row1->addWidget(m_showMainChk);
    row1->addWidget(new QLabel(QStringLiteral("显示数量:"), rows));
    m_showCountSpin = new QSpinBox(rows);
    m_showCountSpin->setRange(1, 5000);
    m_showCountSpin->setValue(500);
    m_showCountSpin->setFixedWidth(70);
    connect(m_showCountSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EnterCalcPage::onShowCountChanged);
    row1->addWidget(m_showCountSpin);
    rowLayout->addLayout(row1);
    auto *row2 = new QHBoxLayout();
    row2->setSpacing(4);
    m_observeChk = new QCheckBox(QStringLiteral("观察完整参数"), rows);
    m_observeChk->setStyleSheet(chkStyle);
    connect(m_observeChk, &QCheckBox::toggled, this, &EnterCalcPage::onObserveToggled);
    row2->addWidget(m_observeChk);
    row2->addWidget(new QLabel(QStringLiteral("显示方式:"), rows));
    m_showModeCombo = new QComboBox(rows);
    m_showModeCombo->addItems({QStringLiteral("主材成本"), QStringLiteral("铜铁"),
                               QStringLiteral("铁芯直径"), QStringLiteral("低压匝数")});
    m_showModeCombo->setFixedWidth(90);
    connect(m_showModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EnterCalcPage::onShowModeChanged);
    row2->addWidget(m_showModeCombo);
    rowLayout->addLayout(row2);
    g1->addWidget(rows);
    m_schemeRibbon->addSeparator();

    // ---- 组2 排序与筛选：左列排序（升/降/取消）+ 右列筛选（筛选/高级/选择/取消）----
    // Ribbon 内容区高约86px，多行堆叠按钮需紧凑化（小图标+文字右置），避免重合
    const auto compact = [](RibbonButton *b) {
        b->setIconSize(QSize(16, 16));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setMinimumSize(70, 20);
        b->setMaximumHeight(20);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        // 覆盖主题 4px 内边距，避免小高度下内容被裁切
        b->setStyleSheet("QToolButton { padding: 0px 4px; }");
    };
    auto *g2 = m_schemeRibbon->addGroup(QStringLiteral("排序与筛选"));
    auto *sortCol = new QVBoxLayout();
    sortCol->setSpacing(1);
    auto *ascBtn = new RibbonButton(QStringLiteral("升序"), ":/icons/sort_asc.svg", g2);
    compact(ascBtn);
    ascBtn->setCheckable(false);
    connect(ascBtn, &QToolButton::clicked, this, [this]() { onSortSchemes(true); });
    sortCol->addWidget(ascBtn);
    auto *descBtn = new RibbonButton(QStringLiteral("降序"), ":/icons/sort_desc.svg", g2);
    compact(descBtn);
    descBtn->setCheckable(false);
    connect(descBtn, &QToolButton::clicked, this, [this]() { onSortSchemes(false); });
    sortCol->addWidget(descBtn);
    auto *sortCancelBtn = new RibbonButton(QStringLiteral("取消"), ":/icons/sort_cancel.svg", g2);
    compact(sortCancelBtn);
    sortCancelBtn->setCheckable(false);
    connect(sortCancelBtn, &QToolButton::clicked, this, &EnterCalcPage::onSortCancel);
    sortCol->addWidget(sortCancelBtn);
    g2->contentLayout()->addLayout(sortCol);
    auto *filterCol = new QVBoxLayout();
    filterCol->setSpacing(1);
    auto *filterBtn = new RibbonButton(QStringLiteral("筛选"), ":/icons/filter.svg", g2);
    compact(filterBtn);
    filterBtn->setCheckable(false);
    connect(filterBtn, &QToolButton::clicked, this, &EnterCalcPage::onFilterSchemes);
    filterCol->addWidget(filterBtn);
    auto *advBtn = new RibbonButton(QStringLiteral("高级"), ":/icons/filter_adv.svg", g2);
    compact(advBtn);
    advBtn->setCheckable(false);
    connect(advBtn, &QToolButton::clicked, this, &EnterCalcPage::onFilterAdvanced);
    filterCol->addWidget(advBtn);
    auto *pickBtn = new RibbonButton(QStringLiteral("选择"), ":/icons/filter_pick.svg", g2);
    compact(pickBtn);
    pickBtn->setCheckable(false);
    connect(pickBtn, &QToolButton::clicked, this, &EnterCalcPage::onFilterPick);
    filterCol->addWidget(pickBtn);
    auto *filterCancelBtn = new RibbonButton(QStringLiteral("取消"), ":/icons/filter_clear.svg", g2);
    compact(filterCancelBtn);
    filterCancelBtn->setCheckable(false);
    connect(filterCancelBtn, &QToolButton::clicked, this, &EnterCalcPage::onFilterCancel);
    filterCol->addWidget(filterCancelBtn);
    g2->contentLayout()->addLayout(filterCol);
    auto *switchCol = new QVBoxLayout();
    switchCol->setSpacing(1);
    auto *switchBtn = new RibbonButton(QStringLiteral("切换"), ":/icons/switch_az.svg", g2);
    compact(switchBtn);
    switchBtn->setCheckable(false);
    connect(switchBtn, &QToolButton::clicked, this, &EnterCalcPage::onSwitchSortColumn);
    switchCol->addWidget(switchBtn);
    auto *findBtn = new RibbonButton(QStringLiteral("查找替换"), ":/icons/find_replace.svg", g2);
    compact(findBtn);
    findBtn->setCheckable(false);
    connect(findBtn, &QToolButton::clicked, this, &EnterCalcPage::onFindScheme);
    switchCol->addWidget(findBtn);
    g2->contentLayout()->addLayout(switchCol);
    m_schemeRibbon->addSeparator();

    // ---- 组3 合并模式 ----
    auto *g3 = m_schemeRibbon->addGroup(QStringLiteral("合并模式"));
    auto *mergeSetBtn = new RibbonButton(QStringLiteral("设置合并模式"), ":/icons/merge_set.svg", g3);
    mergeSetBtn->setCheckable(false);
    connect(mergeSetBtn, &QToolButton::clicked, this, &EnterCalcPage::onMergeSet);
    g3->addButton(mergeSetBtn);
    auto *mergeToggleBtn = new RibbonButton(QStringLiteral("切换合并模式"), ":/icons/merge_toggle.svg", g3);
    mergeToggleBtn->setCheckable(false);
    connect(mergeToggleBtn, &QToolButton::clicked, this, &EnterCalcPage::onMergeToggle);
    g3->addButton(mergeToggleBtn);
    m_schemeRibbon->addSeparator();

    // ---- 组4 方案库比较 ----
    auto *g4 = m_schemeRibbon->addGroup(QStringLiteral("方案库比较"));
    auto *compareBtn = new RibbonButton(QStringLiteral("方案库比较"), ":/icons/compare.svg", g4);
    compareBtn->setCheckable(false);
    connect(compareBtn, &QToolButton::clicked, this, &EnterCalcPage::onCompareLibrary);
    g4->addButton(compareBtn);
    m_schemeRibbon->addSeparator();

    // ---- 组5 方案选择：方案库/方案序号 + 方案确认 ----
    auto *g5 = m_schemeRibbon->addGroup(QStringLiteral("方案选择"));
    auto *selWidget = new QWidget(g5);
    auto *selLayout = new QVBoxLayout(selWidget);
    selLayout->setContentsMargins(0, 0, 0, 0);
    selLayout->setSpacing(6);
    auto *libRow = new QHBoxLayout();
    libRow->setSpacing(4);
    libRow->addWidget(new QLabel(QStringLiteral("选择方案库:"), selWidget));
    m_libIndexSpin = new QSpinBox(selWidget);
    m_libIndexSpin->setRange(1, 99);
    m_libIndexSpin->setValue(1);
    m_libIndexSpin->setFixedWidth(70);
    connect(m_libIndexSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EnterCalcPage::onLibIndexChanged);
    libRow->addWidget(m_libIndexSpin);
    selLayout->addLayout(libRow);
    auto *schemeRow = new QHBoxLayout();
    schemeRow->setSpacing(4);
    schemeRow->addWidget(new QLabel(QStringLiteral("选择方案:"), selWidget));
    m_schemeIndexSpin = new QSpinBox(selWidget);
    m_schemeIndexSpin->setRange(1, 9999);
    m_schemeIndexSpin->setValue(1);
    m_schemeIndexSpin->setFixedWidth(70);
    connect(m_schemeIndexSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EnterCalcPage::onSchemeIndexChanged);
    schemeRow->addWidget(m_schemeIndexSpin);
    selLayout->addLayout(schemeRow);
    g5->addWidget(selWidget);
    auto *confirmBtn = new RibbonButton(QStringLiteral("方案确认"), ":/icons/confirm.svg", g5);
    confirmBtn->setCheckable(false);
    connect(confirmBtn, &QToolButton::clicked, this, &EnterCalcPage::onConfirmScheme);
    g5->addButton(confirmBtn);
}

void EnterCalcPage::buildPrintRibbon()
{
    auto *g1 = m_printRibbon->addGroup(QStringLiteral("打印"));
    auto *b1 = new RibbonButton(QStringLiteral("快速打印"), ":/icons/print_fast.svg", g1);
    b1->setCheckable(false);
    connect(b1, &QToolButton::clicked, this, &EnterCalcPage::onQuickPrint);
    g1->addButton(b1);
    auto *b2 = new RibbonButton(QStringLiteral("打印"), ":/icons/print.svg", g1);
    b2->setCheckable(false);
    connect(b2, &QToolButton::clicked, this, &EnterCalcPage::onPrint);
    g1->addButton(b2);
    auto *b3 = new RibbonButton(QStringLiteral("打印预览"), ":/icons/preview.svg", g1);
    b3->setCheckable(false);
    connect(b3, &QToolButton::clicked, this, &EnterCalcPage::onPrintPreview);
    g1->addButton(b3);
    m_printRibbon->addSeparator();

    auto *g2 = m_printRibbon->addGroup(QStringLiteral("输出Excel"));
    auto *b4 = new RibbonButton(QStringLiteral("输出外部文件"), ":/icons/export.svg", g2);
    b4->setCheckable(false);
    connect(b4, &QToolButton::clicked, this, &EnterCalcPage::onExportCsv);
    g2->addButton(b4);
    auto *b5 = new RibbonButton(QStringLiteral("保存为计算单"), ":/icons/save.svg", g2);
    b5->setCheckable(false);
    connect(b5, &QToolButton::clicked, this, &EnterCalcPage::onSaveCalcSheet);
    g2->addButton(b5);
}

void EnterCalcPage::setupOptimizeTab()
{
    auto *tab = new QWidget();
    auto *layout = new QHBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 竖排"程序选择"导航按钮（点击返回主界面），顶格放置于侧边栏左侧
    m_navButton = new QPushButton(QStringLiteral("程\n序\n选\n择"), tab);
    m_navButton->setCursor(Qt::PointingHandCursor);
    m_navButton->setToolTip(QStringLiteral("返回主界面"));
    m_navButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_navButton->setStyleSheet(
        "QPushButton { background: rgba(0,188,212,0.15); color: #4dd0e1;"
        "border: none; border-right: 1px solid #3a4050; border-radius: 0px;"
        "font-size: 12px; padding: 4px; }"
        "QPushButton:hover { background: #00bcd4; color: #0d1117; }");
    connect(m_navButton, &QPushButton::clicked, this, &EnterCalcPage::navigateBack);
    layout->addWidget(m_navButton);

    // Left sidebar
    auto *sidebar = new SidebarPanel(tab);
    sidebar->addButton(QStringLiteral("选用推荐方案"), ":/icons/recommend.svg");
    sidebar->addButton(QStringLiteral("保存为我的方案"), ":/icons/save_scheme.svg");
    sidebar->addButton(QStringLiteral("从方案库中选择"), ":/icons/library.svg");
    sidebar->addButton(QStringLiteral("返回上一次方案"), ":/icons/undo.svg");
    sidebar->addButton(QStringLiteral("下一步"), ":/icons/enter_calc.svg");
    sidebar->addButton(QStringLiteral("取消"), ":/icons/stop.svg");
    // 取消按钮(index 5)返回优化计算参数设置页
    connect(sidebar, &SidebarPanel::buttonClicked, this, [this](int index) {
        if (index == 5)
            emit navigateBack();
    });
    layout->addWidget(sidebar);

    // Structure config table
    auto *table = new QTableWidget(tab);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"#", "结构名称", "选项", "备注"});
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 30);
    table->setColumnWidth(1, 180);
    table->setColumnWidth(2, 150);

    QStringList sections = {"变压器类型选择", "一 铁芯结构", "二 线圈结构", "三 装配结构", "四 工艺方案", "五 其他参数"};
    QVector<QStringList> items = {
        {"变压器类型:配电变压器"},
        {"铁芯结构形式:类椭圆形", "铁扼结构形式:D形扼", "铁芯轭截面放大系数:1.0",
         "铁芯柱截面放大系数:1.0", "铁芯有效截面积系数:0.97"},
        {"绕组形式:双绕组", "低压线圈结构形式:箔绕", "高压线圈结构形式:多层圆筒",
         "低压线圈材料:铜", "高压线圈材料:铜", "线圈绝缘类型:标准绝缘"},
        {"带储油柜:是"},
        {"采用压包方案:否", "采用真空注油:是", "采用干燥处理:是", "采用热套工艺:否"},
        {"立体卷截面计算方式:按叠片", "铁芯叠片系数:0.97", "附加损耗系数:1.0",
         "杂散损耗系数:1.0", "温升计算方式:标准计算", "短路阻抗计算方式:标准计算"}
    };

    int row = 0;
    for (int s = 0; s < sections.size(); ++s) {
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        auto *hdr = new QTableWidgetItem(sections[s]);
        QFont f = hdr->font(); f.setBold(true); hdr->setFont(f);
        hdr->setBackground(QColor("#1a3a4a"));
        table->setItem(row, 1, hdr);
        table->setSpan(row, 1, 1, 3);
        row++;
        for (const auto &item : items[s]) {
            auto parts = item.split(':');
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
            table->setItem(row, 1, new QTableWidgetItem(parts[0]));
            table->setItem(row, 2, new QTableWidgetItem(parts.size() > 1 ? parts[1] : ""));
            table->setItem(row, 3, new QTableWidgetItem(""));
            row++;
        }
    }

    // 主区分割：左结构配置表，右电磁计算结果面板
    auto *splitter = new QSplitter(Qt::Horizontal, tab);
    splitter->setHandleWidth(1);
    splitter->addWidget(table);
    m_emResultPanel = new EmResultPanel(splitter);
    splitter->addWidget(m_emResultPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({ 500, 700 });
    layout->addWidget(splitter, 1);
    m_stack->addWidget(tab);
}

void EnterCalcPage::setupSchemeTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    m_schemeTable = new SchemeTableWidget(tab);
    layout->addWidget(m_schemeTable);
    m_stack->addWidget(tab);
}

void EnterCalcPage::setupPrintTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    m_printTable = new PrintTableWidget(tab);
    m_printTable->loadData(m_engine.calculate(m_params, m_config));
    layout->addWidget(m_printTable);
    m_stack->addWidget(tab);
}

void EnterCalcPage::onTabChanged(int index)
{
    m_optimizeRibbon->setVisible(index == 0);
    m_schemeRibbon->setVisible(index == 1);
    m_printRibbon->setVisible(index == 2);
    m_stack->setCurrentIndex(index);
}

void EnterCalcPage::onRunEmCalc()
{
    CalcInput input;   // 默认设计变量（SB20-M-630-10）
    if (!m_engine.calcElectromagnetic(input, m_emResult) || !m_emResult.valid) {
        m_statusBar->setText(QStringLiteral("电磁计算失败: %1").arg(m_emResult.error));
        QMessageBox::warning(this, QStringLiteral("电磁计算"),
                             QStringLiteral("计算失败: %1").arg(m_emResult.error));
        return;
    }
    m_hasResult = true;

    // 结果面板 + 打印表 + 方案入库
    m_emResultPanel->loadResult(m_emResult);
    m_printTable->loadData(m_engine.calculate(m_params, m_config));
    appendScheme(input, m_emResult);

    m_statusBar->setText(
        QStringLiteral("电磁计算完成：空载损耗 %1 W | 负载损耗 %2 W | 阻抗电压 %3% | "
                       "油面温升 %4 K | 总重 %5 kg | 材料成本 %6 元")
            .arg(QString::number(m_emResult.core.noLoadLoss_W, 'f', 0),
                 QString::number(m_emResult.winding.loadLoss_W, 'f', 0),
                 QString::number(m_emResult.impedance.impedance_pct, 'f', 2),
                 QString::number(m_emResult.thermal.oilRise_K, 'f', 1),
                 QString::number(m_emResult.mass.totalWeight_kg, 'f', 0),
                 QString::number(m_emResult.cost.materialCost, 'f', 0)));
}

void EnterCalcPage::onSelfTest()
{
    const QString report = ElectromagneticEngine::selfTestReport();
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("对拍自检（SB20-M-630-10 计算单缓存值）"));
    box.setText(report);
    box.setTextFormat(Qt::PlainText);
    box.setIcon(QMessageBox::Information);
    box.setFont(QFont(QStringLiteral("Consolas")));
    box.exec();
    m_statusBar->setText(QStringLiteral("对拍自检已执行，详见弹窗报告"));
}

void EnterCalcPage::onSaveCalcSheet()
{
    if (!m_hasResult) {
        // 未计算过时先执行一次默认输入的全链路计算
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存计算单"),
        QStringLiteral("SB20-M-630-10计算单.txt"),
        QStringLiteral("文本文件 (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存计算单"),
                             QStringLiteral("无法写入文件: %1").arg(path));
        return;
    }
    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << EmResultPanel::resultText(m_emResult) << "\n";
    file.close();
    m_statusBar->setText(QStringLiteral("计算单已保存: %1").arg(path));
}

namespace {

// 表格分页绘制到打印机（表头每页重复，列宽按打印区宽度等比）
void paintTableToPrinter(QPrinter *printer, QTableWidget *table)
{
    QPainter painter(printer);
    const QRectF page = printer->pageRect(QPrinter::DevicePixel);
    const int rows = table->rowCount();
    const int cols = table->columnCount();

    // 列宽比例：按表格当前可见列宽
    QVector<double> ratios(cols, 0.0);
    double totalRatio = 0.0;
    for (int c = 0; c < cols; ++c) {
        ratios[c] = table->isColumnHidden(c) ? 0.0 : table->columnWidth(c);
        totalRatio += ratios[c];
    }
    if (totalRatio <= 0.0) {
        return;
    }
    for (int c = 0; c < cols; ++c) {
        ratios[c] /= totalRatio;
    }

    const QFont bodyFont = table->font();
    QFont bFont = bodyFont;
    bFont.setBold(true);
    const QFontMetrics fmBody(bodyFont);
    const QFontMetrics fmHead(bFont);
    const double rowH = fmBody.height() + 10.0;
    const double headH = fmHead.height() + 10.0;

    auto drawRow = [&](int row, double y, bool header) {
        double x = page.left();
        for (int c = 0; c < cols; ++c) {
            const double w = ratios[c] * page.width();
            if (w <= 0.0) {
                continue;
            }
            QString text;
            if (header) {
                text = table->horizontalHeaderItem(c)
                           ? table->horizontalHeaderItem(c)->text() : QString();
                text.replace(QLatin1Char('\n'), QLatin1Char(' '));
            } else {
                const QTableWidgetItem *it = table->item(row, c);
                text = it ? it->text() : QString();
            }
            painter.setFont(header ? bFont : bodyFont);
            painter.drawText(QRectF(x + 3, y, w - 6, header ? headH : rowH),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             text);
            painter.drawLine(QPointF(x, y + (header ? headH : rowH)),
                             QPointF(x + w, y + (header ? headH : rowH)));
            painter.drawLine(QPointF(x, y), QPointF(x, y + (header ? headH : rowH)));
            x += w;
        }
        painter.drawLine(QPointF(page.right(), y),
                         QPointF(page.right(), y + (header ? headH : rowH)));
    };

    double y = page.top();
    drawRow(-1, y, true);
    y += headH;
    for (int r = 0; r < rows; ++r) {
        if (y + rowH > page.bottom()) {
            printer->newPage();
            y = page.top();
            drawRow(-1, y, true);
            y += headH;
        }
        if (!table->isRowHidden(r)) {
            drawRow(r, y, false);
            y += rowH;
        }
    }
    painter.end();
}

} // namespace

// ---- 方案选择 Tab ----

void EnterCalcPage::onSortSchemes(bool ascending)
{
    if (m_schemeTable->rowCount() == 0) {
        m_statusBar->setText(QStringLiteral("暂无方案可排序，请先执行计算"));
        return;
    }
    m_schemeTable->sortItems(m_sortCol, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
    const auto *head = m_schemeTable->horizontalHeaderItem(m_sortCol);
    m_statusBar->setText(QStringLiteral("方案已按%1%2排序")
                             .arg(head ? head->text().replace(QLatin1Char('\n'), QLatin1Char(' '))
                                       : QString())
                             .arg(ascending ? QStringLiteral("升序") : QStringLiteral("降序")));
}

void EnterCalcPage::onSortCancel()
{
    if (m_schemeTable->rowCount() == 0) {
        m_statusBar->setText(QStringLiteral("暂无方案，无需取消排序"));
        return;
    }
    // 按方案序号列恢复原始顺序
    m_schemeTable->sortItems(1, Qt::AscendingOrder);
    m_statusBar->setText(QStringLiteral("已取消排序，恢复方案序号顺序"));
}

void EnterCalcPage::onFilterSchemes()
{
    if (m_schemeTable->rowCount() == 0) {
        m_statusBar->setText(QStringLiteral("暂无方案可筛选，请先执行计算"));
        return;
    }
    bool ok = false;
    const double limit = QInputDialog::getDouble(
        this, QStringLiteral("筛选方案"),
        QStringLiteral("主材成本上限（元，取消则清除筛选）："),
        0.0, 0.0, 1e9, 1, &ok);
    int shown = 0;
    for (int r = 0; r < m_schemeTable->rowCount(); ++r) {
        const bool visible = ok
            && m_schemeTable->item(r, 2)
            && m_schemeTable->item(r, 2)->text().toDouble() <= limit;
        m_schemeTable->setRowHidden(r, ok ? !visible : false);
        if (!m_schemeTable->isRowHidden(r)) {
            ++shown;
        }
    }
    m_statusBar->setText(ok
        ? QStringLiteral("筛选：主材成本 ≤ %1，共 %2/%3 个方案")
              .arg(QString::number(limit, 'f', 1)).arg(shown)
              .arg(m_schemeTable->rowCount())
        : QStringLiteral("已清除方案筛选"));
}

void EnterCalcPage::onSchemeColumnMenu()
{
    QMenu menu(this);
    for (int c = 1; c < m_schemeTable->columnCount(); ++c) {
        const auto *head = m_schemeTable->horizontalHeaderItem(c);
        if (!head) {
            continue;
        }
        auto *act = menu.addAction(head->text().replace(QLatin1Char('\n'), QLatin1Char(' ')));
        act->setCheckable(true);
        act->setChecked(!m_schemeTable->isColumnHidden(c));
        connect(act, &QAction::toggled, this, [this, c](bool on) {
            m_schemeTable->setColumnHidden(c, !on);
        });
    }
    menu.exec(QCursor::pos());
    m_statusBar->setText(QStringLiteral("显示选项已更新"));
}

void EnterCalcPage::onConfirmScheme()
{
    const int row = m_schemeTable->currentRow();
    if (row < 0 || m_schemeTable->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("方案确认"),
                                 QStringLiteral("请先在方案表中选择一个方案"));
        return;
    }
    m_confirmedRow = row;
    const QString idx = m_schemeTable->item(row, 1)
                            ? m_schemeTable->item(row, 1)->text() : QString('?');
    if (m_schemeIndexSpin) {
        m_schemeIndexSpin->blockSignals(true);
        m_schemeIndexSpin->setValue(idx.toInt());
        m_schemeIndexSpin->blockSignals(false);
    }
    m_statusBar->setText(QStringLiteral("已确认方案 %1，跳转输出打印").arg(idx));
    m_tabBar->setCurrentIndex(2);
}

void EnterCalcPage::onFilterAdvanced()
{
    if (m_schemeTable->rowCount() == 0) {
        m_statusBar->setText(QStringLiteral("暂无方案可筛选，请先执行计算"));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("高级筛选"));
    auto *form = new QVBoxLayout(&dlg);
    auto *costSpin = new QDoubleSpinBox(&dlg);
    costSpin->setRange(0.0, 1e9);
    costSpin->setDecimals(1);
    costSpin->setSuffix(QStringLiteral(" 元"));
    costSpin->setValue(0.0);
    form->addWidget(new QLabel(QStringLiteral("主材成本上限（0=不限）："), &dlg));
    form->addWidget(costSpin);
    auto *dMinSpin = new QDoubleSpinBox(&dlg);
    dMinSpin->setRange(0.0, 2000.0);
    dMinSpin->setDecimals(0);
    dMinSpin->setSuffix(QStringLiteral(" mm"));
    form->addWidget(new QLabel(QStringLiteral("铁芯直径下限（0=不限）："), &dlg));
    form->addWidget(dMinSpin);
    auto *dMaxSpin = new QDoubleSpinBox(&dlg);
    dMaxSpin->setRange(0.0, 2000.0);
    dMaxSpin->setDecimals(0);
    dMaxSpin->setValue(2000.0);
    dMaxSpin->setSuffix(QStringLiteral(" mm"));
    form->addWidget(new QLabel(QStringLiteral("铁芯直径上限："), &dlg));
    form->addWidget(dMaxSpin);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted) {
        m_statusBar->setText(QStringLiteral("已取消高级筛选"));
        return;
    }
    const double costMax = costSpin->value();
    const double dMin = dMinSpin->value();
    const double dMax = dMaxSpin->value();
    int shown = 0;
    for (int r = 0; r < m_schemeTable->rowCount(); ++r) {
        const double cost = m_schemeTable->item(r, 2)
                                ? m_schemeTable->item(r, 2)->text().toDouble() : 0.0;
        const double dia = m_schemeTable->item(r, 4)
                               ? m_schemeTable->item(r, 4)->text().toDouble() : 0.0;
        const bool ok = (costMax <= 0.0 || cost <= costMax)
                        && (dMin <= 0.0 || dia >= dMin) && dia <= dMax;
        m_schemeTable->setRowHidden(r, !ok);
        if (ok) {
            ++shown;
        }
    }
    m_statusBar->setText(QStringLiteral("高级筛选：成本≤%1、直径 %2~%3，共 %4/%5 个方案")
                             .arg(QString::number(costMax, 'f', 1))
                             .arg(QString::number(dMin, 'f', 0))
                             .arg(QString::number(dMax, 'f', 0))
                             .arg(shown).arg(m_schemeTable->rowCount()));
}

void EnterCalcPage::onFilterPick()
{
    if (m_schemeTable->rowCount() == 0) {
        m_statusBar->setText(QStringLiteral("暂无方案可筛选，请先执行计算"));
        return;
    }
    QStringList cols;
    for (int c = 1; c < m_schemeTable->columnCount(); ++c) {
        const auto *head = m_schemeTable->horizontalHeaderItem(c);
        cols << (head ? head->text().replace(QLatin1Char('\n'), QLatin1Char(' ')) : QString());
    }
    bool ok = false;
    const QString colName = QInputDialog::getItem(
        this, QStringLiteral("选择筛选"), QStringLiteral("筛选列："), cols, 0, false, &ok);
    if (!ok) {
        return;
    }
    const int col = cols.indexOf(colName) + 1;
    const QString val = QInputDialog::getText(
        this, QStringLiteral("选择筛选"),
        QStringLiteral("“%1”等于：").arg(colName), QLineEdit::Normal, QString(), &ok);
    if (!ok || val.isEmpty()) {
        return;
    }
    int shown = 0;
    for (int r = 0; r < m_schemeTable->rowCount(); ++r) {
        const bool hit = m_schemeTable->item(r, col)
                         && m_schemeTable->item(r, col)->text() == val;
        m_schemeTable->setRowHidden(r, !hit);
        if (hit) {
            ++shown;
        }
    }
    m_statusBar->setText(QStringLiteral("选择筛选：%1 = %2，共 %3 个方案")
                             .arg(colName, val).arg(shown));
}

void EnterCalcPage::onFilterCancel()
{
    for (int r = 0; r < m_schemeTable->rowCount(); ++r) {
        m_schemeTable->setRowHidden(r, false);
    }
    m_statusBar->setText(QStringLiteral("已清除全部筛选"));
}

void EnterCalcPage::onSwitchSortColumn()
{
    const QList<int> cols = {2, 3, 4, 6};
    const int i = cols.indexOf(m_sortCol);
    m_sortCol = cols[(i + 1) % cols.size()];
    const auto *head = m_schemeTable->horizontalHeaderItem(m_sortCol);
    m_statusBar->setText(QStringLiteral("排序列已切换为：%1")
                             .arg(head ? head->text().replace(QLatin1Char('\n'), QLatin1Char(' '))
                                       : QString()));
}

void EnterCalcPage::onFindScheme()
{
    if (m_schemeTable->rowCount() == 0) {
        m_statusBar->setText(QStringLiteral("暂无方案可查找，请先执行计算"));
        return;
    }
    bool ok = false;
    const int idx = QInputDialog::getInt(
        this, QStringLiteral("查找方案"), QStringLiteral("方案序号："),
        1, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }
    for (int r = 0; r < m_schemeTable->rowCount(); ++r) {
        if (m_schemeTable->item(r, 1)
            && m_schemeTable->item(r, 1)->text().toInt() == idx) {
            m_schemeTable->selectRow(r);
            m_schemeTable->scrollToItem(m_schemeTable->item(r, 0));
            m_statusBar->setText(QStringLiteral("已定位方案 %1（第 %2 行）").arg(idx).arg(r + 1));
            return;
        }
    }
    m_statusBar->setText(QStringLiteral("未找到方案 %1").arg(idx));
}

void EnterCalcPage::onMergeSet()
{
    // 设置合并模式：选择合并显示的主要参数列
    onSchemeColumnMenu();
}

void EnterCalcPage::onMergeToggle()
{
    m_mergeOn = !m_mergeOn;
    const QList<int> mainCols = {1, 2, 4, 6, 7, 8, 9, 10, 11};
    for (int c = 1; c < m_schemeTable->columnCount(); ++c) {
        m_schemeTable->setColumnHidden(c, m_mergeOn && !mainCols.contains(c));
    }
    if (m_showMainChk) {
        m_showMainChk->blockSignals(true);
        m_showMainChk->setChecked(m_mergeOn);
        m_showMainChk->blockSignals(false);
    }
    m_statusBar->setText(m_mergeOn ? QStringLiteral("合并模式：开（仅主要参数列）")
                                   : QStringLiteral("合并模式：关（全部参数列）"));
}

void EnterCalcPage::onCompareLibrary()
{
    if (m_confirmedRow < 0 || m_schemeTable->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("方案库比较"),
                                 QStringLiteral("尚无已确认方案，请先在方案表中选择并确认一个方案"));
        return;
    }
    const int cur = m_schemeTable->currentRow();
    if (cur < 0 || cur == m_confirmedRow) {
        QMessageBox::information(this, QStringLiteral("方案库比较"),
                                 QStringLiteral("请先在方案表中选中待比较的方案（当前选中即已确认方案）"));
        return;
    }
    const QList<int> cols = {2, 3, 4, 6, 7, 8, 9, 10, 11};
    QString text = QStringLiteral("参数\t当前方案\t已确认方案\n");
    for (int c : cols) {
        const auto *head = m_schemeTable->horizontalHeaderItem(c);
        const QString name = head ? head->text().replace(QLatin1Char('\n'), QLatin1Char(' ')) : QString();
        const QString a = m_schemeTable->item(cur, c) ? m_schemeTable->item(cur, c)->text() : QString();
        const QString b = m_schemeTable->item(m_confirmedRow, c)
                              ? m_schemeTable->item(m_confirmedRow, c)->text() : QString();
        text += name + QLatin1Char('\t') + a + QLatin1Char('\t') + b + QLatin1Char('\n');
    }
    QMessageBox::information(this, QStringLiteral("方案库比较"), text);
}

void EnterCalcPage::onShowCountChanged(int count)
{
    for (int r = 0; r < m_schemeTable->rowCount(); ++r) {
        m_schemeTable->setRowHidden(r, r >= count);
    }
    m_statusBar->setText(QStringLiteral("显示数量：%1（共 %2 个方案）")
                             .arg(count).arg(m_schemeTable->rowCount()));
}

void EnterCalcPage::onShowModeChanged(int index)
{
    const QList<int> cols = {2, 3, 4, 6};
    if (index >= 0 && index < cols.size()) {
        m_sortCol = cols[index];
    }
    const auto *head = m_schemeTable->horizontalHeaderItem(m_sortCol);
    m_statusBar->setText(QStringLiteral("显示方式：%1（升/降序将按此列排序）")
                             .arg(head ? head->text().replace(QLatin1Char('\n'), QLatin1Char(' '))
                                       : QString()));
}

void EnterCalcPage::onObserveToggled(bool checked)
{
    if (!checked) {
        return;
    }
    for (int c = 1; c < m_schemeTable->columnCount(); ++c) {
        m_schemeTable->setColumnHidden(c, false);
    }
    if (m_showMainChk) {
        m_showMainChk->blockSignals(true);
        m_showMainChk->setChecked(false);
        m_showMainChk->blockSignals(false);
    }
    m_statusBar->setText(QStringLiteral("观察完整参数：已显示全部参数列"));
}

void EnterCalcPage::onLibIndexChanged(int value)
{
    m_statusBar->setText(QStringLiteral("已选择方案库 %1（当前会话仅方案库 1 可用）").arg(value));
}

void EnterCalcPage::onSchemeIndexChanged(int value)
{
    const int row = value - 1;
    if (row < 0 || row >= m_schemeTable->rowCount()) {
        m_statusBar->setText(QStringLiteral("方案 %1 不存在（共 %2 个方案）")
                                 .arg(value).arg(m_schemeTable->rowCount()));
        return;
    }
    m_schemeTable->selectRow(row);
    m_schemeTable->scrollToItem(m_schemeTable->item(row, 0));
    m_statusBar->setText(QStringLiteral("已选中方案 %1").arg(value));
}

// ---- 输出打印 Tab ----

void EnterCalcPage::onQuickPrint()
{
    QPrinter printer(QPrinter::HighResolution);
    if (!printer.isValid()) {
        QMessageBox::warning(this, QStringLiteral("快速打印"),
                             QStringLiteral("未找到可用打印机，请检查打印机连接"));
        return;
    }
    paintTableToPrinter(&printer, m_printTable);
    m_statusBar->setText(QStringLiteral("快速打印已发送至打印机: %1")
                             .arg(printer.printerName()));
}

void EnterCalcPage::onPrint()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(QStringLiteral("打印计算单"));
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    paintTableToPrinter(&printer, m_printTable);
    m_statusBar->setText(QStringLiteral("打印已发送至打印机: %1")
                             .arg(printer.printerName()));
}

void EnterCalcPage::onPrintPreview()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle(QStringLiteral("打印预览 - 计算单"));
    preview.resize(900, 700);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this,
            [this](QPrinter *p) { paintTableToPrinter(p, m_printTable); });
    preview.exec();
    m_statusBar->setText(QStringLiteral("打印预览已关闭"));
}

void EnterCalcPage::onExportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("输出外部文件"),
        QStringLiteral("计算单.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("输出外部文件"),
                             QStringLiteral("无法写入文件: %1").arg(path));
        return;
    }
    // UTF-8 BOM：保证 Excel 直接打开不乱码
    file.write("\xEF\xBB\xBF");
    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    for (int r = 0; r < m_printTable->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_printTable->columnCount(); ++c) {
            const QTableWidgetItem *it = m_printTable->item(r, c);
            // CSV 转义：包含逗号/引号时加引号并转义内部引号
            QString text = it ? it->text() : QString();
            if (text.contains(QLatin1Char(',')) || text.contains(QLatin1Char('"'))) {
                text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
                text = QLatin1Char('"') + text + QLatin1Char('"');
            }
            cells << text;
        }
        ts << cells.join(QLatin1Char(',')) << "\n";
    }
    file.close();
    m_statusBar->setText(QStringLiteral("CSV 已导出: %1").arg(path));
}

void EnterCalcPage::appendScheme(const CalcInput &input, const CalcResult &result)
{
    OptimizationResult scheme;
    scheme.schemeIdx = m_schemeTable->rowCount() + 1;
    scheme.costCuFeOil = result.cost.materialCost;
    scheme.costCuFe = result.cost.steelCost + result.cost.hvWireCost
                      + result.cost.lvWireCost;
    scheme.coreD = input.coreDiameter_mm;
    scheme.coreL = result.core.minorAxis_mm;
    scheme.lvTurns = input.lvTurns;
    scheme.lvRuleT = input.lvFoilThick_mm;
    scheme.lvRuleW = input.lvFoilWidth_mm;
    scheme.hvRuleT = input.hvBareThick_mm;
    scheme.hvRuleW = input.hvBareWidth_mm;
    scheme.hvLayers = result.winding.layerCount;
    scheme.lvOilDucts = 5;
    scheme.hvOilDucts = 5;
    scheme.lvToYoke = input.lvEndInsul_mm;
    scheme.mainDuct = result.winding.mainDuct_mm;
    m_schemeTable->addResult(scheme);
}
