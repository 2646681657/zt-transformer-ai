#include "EnterCalcPage.h"
#include "PrintOutputData.h"
#include "RibbonBar.h"
#include "RibbonGroup.h"
#include "RibbonButton.h"
#include "SidebarPanel.h"
#include "SchemeTableWidget.h"
#include "PrintTableWidget.h"
#include "EmResultPanel.h"
#include "GridOptimizer.h"
#include "SchemeConstraints.h"
#include "SchemeStore.h"
#include "RecommendSchemes.h"
#include "SchemePickDialog.h"
#include "SelfLearnDialog.h"
#include "AiAnalysisDialog.h"
#include "QuoteCalculator.h"
#include <QDateTime>
#include <QDir>
#include <algorithm>
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
#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPageLayout>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPainter>
#include <QFormLayout>
#include <QPlainTextEdit>

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

    // 网格寻优器（后台线程执行，候选方案与进度经信号回主线程）
    m_optimizer = new GridOptimizer(this);
    connect(m_optimizer, &IOptimizer::progressUpdated,
            this, &EnterCalcPage::onOptimizeProgress);
    connect(m_optimizer, &IOptimizer::candidateReady,
            this, &EnterCalcPage::onOptimizeCandidate);
    connect(m_optimizer, &IOptimizer::finished,
            this, &EnterCalcPage::onOptimizeFinished);
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
    // AI 解读：当前计算结果 → LLM 解读文字（阶段三）
    auto *aiBtn = new RibbonButton(QStringLiteral("AI 解读"), ":/icons/memory_on.svg", g0);
    aiBtn->setCheckable(false);
    connect(aiBtn, &QToolButton::clicked, this, [this]() {
        if (!m_hasResult) {
            QMessageBox::information(this, QStringLiteral("暂无结果"),
                QStringLiteral("请先执行快速计算或寻优计算，再使用 AI 解读"));
            return;
        }
        const QString data = QStringLiteral(
            "变压器电磁计算结果（引擎输出）：\n"
            "容量: %1 kVA；高压/低压: %2/%3 kV\n"
            "空载损耗: %4 W；负载损耗: %5 W；阻抗电压: %6%\n"
            "心柱磁密: %7 T；铁轭磁密: %8 T\n"
            "空载电流: %9%\n"
            "硅钢片总重: %10 kg；导线总重: %11 kg\n"
            "油面温升: %12 K；高压绕组温升: %13 K；低压绕组温升: %14 K\n"
            "高压电密: %15 A/mm²；低压电密: %16 A/mm²\n"
            "变压器总重: %17 kg；材料成本: %18 元\n"
            "性能标准：空载损耗标准 %19 W，负载损耗标准 %20 W，"
            "阻抗电压标准 %21%，空载电流标准 %22%\n"
            "约束校验：%23")
            .arg(m_params.capacity_kVA).arg(m_params.hvRatedVoltage_kV).arg(m_params.lvRatedVoltage_kV)
            .arg(m_emResult.core.noLoadLoss_W, 0, 'f', 0)
            .arg(m_emResult.winding.loadLoss_W, 0, 'f', 0)
            .arg(m_emResult.impedance.impedance_pct, 0, 'f', 2)
            .arg(m_emResult.core.fluxDensity_core_T, 0, 'f', 3)
            .arg(m_emResult.core.fluxDensity_yoke_T, 0, 'f', 3)
            .arg(m_emResult.core.noLoadCurrent_pct, 0, 'f', 2)
            .arg(m_emResult.core.coreWeight_kg, 0, 'f', 0)
            .arg(m_emResult.winding.wireWeightTotal_kg, 0, 'f', 0)
            .arg(m_emResult.thermal.oilRise_K, 0, 'f', 1)
            .arg(m_emResult.thermal.hvWindingRise_K, 0, 'f', 1)
            .arg(m_emResult.thermal.lvWindingRise_K, 0, 'f', 1)
            .arg(m_emResult.winding.hvCurrentDensity, 0, 'f', 2)
            .arg(m_emResult.winding.lvCurrentDensity, 0, 'f', 2)
            .arg(m_emResult.mass.totalWeight_kg, 0, 'f', 0)
            .arg(m_emResult.cost.materialCost, 0, 'f', 0)
            .arg(m_params.noLoadLossStd_W, 0, 'f', 0)
            .arg(m_params.loadLossStd_W, 0, 'f', 0)
            .arg(m_params.impedanceVoltageStd_pct, 0, 'f', 2)
            .arg(m_params.noLoadCurrentStd_pct, 0, 'f', 2)
            .arg([&]() {
                const auto check = checkSchemeConstraints(m_params, m_emResult);
                return check.passed ? QStringLiteral("全部通过")
                                    : check.violations.join(QStringLiteral("；"));
            }());
        AiAnalysisDialog dlg(QStringLiteral("AI 计算结果解读"),
            QStringLiteral("任务：解读该电磁计算结果，评估各项指标是否达标、"
                           "哪些指标偏离标准较大，并给出调整设计变量的方向建议。"),
            data, this);
        dlg.exec();
    });
    g0->addButton(aiBtn);
    m_optimizeRibbon->addSeparator();

    auto *g1 = m_optimizeRibbon->addGroup(QStringLiteral("初始化设置(从左到右顺序查看)"));
    // 只读查看入口：展示各项配置当前值（编辑入口在参数设置页，寻优前可在此核对）
    const QStringList initButtons = {
        QStringLiteral("产品结构"), QStringLiteral("材料选择"),
        QStringLiteral("修正参数"), QStringLiteral("循环参数"),
        QStringLiteral("约束条件"), QStringLiteral("初始化信息")
    };
    for (int i = 0; i < initButtons.size(); ++i) {
        auto *btn = new RibbonButton(initButtons[i],
            QStringLiteral(":/icons/%1.svg").arg(
                QStringList{"product", "material", "adjust",
                            "loop_param", "constraint", "init_info"}[i]), g1);
        btn->setCheckable(false);
        const int index = i;
        connect(btn, &QToolButton::clicked, this,
                [this, index]() { showInitInfoDialog(index); });
        g1->addButton(btn);
    }
    m_optimizeRibbon->addSeparator();

    auto *gMem = m_optimizeRibbon->addGroup(QStringLiteral("学习记忆"));
    auto *memBtn = new RibbonButton(QStringLiteral("打开记忆功能"), ":/icons/memory_on.svg", gMem);
    memBtn->setCheckable(false);
    // 从记忆库选择方案：应用后立即重新计算（与侧边栏方案库选择同链路）
    connect(memBtn, &QToolButton::clicked, this, [this]() {
        QVector<SchemeStore::SchemeEntry> entries =
            SchemeStore::loadEntries(SchemeStore::memorySchemesPath());
        if (entries.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("记忆库为空"),
                QStringLiteral("暂无使用记录，进入计算后将自动记录方案"));
            return;
        }
        SchemePickDialog dlg(QStringLiteral("从记忆库中选择"), entries,
                             SchemeStore::memorySchemesPath(), this);
        if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
            m_calcInput = dlg.selectedEntry().input;
            onRunEmCalc();
        }
    });
    gMem->addButton(memBtn);

    // AI 自学习：设计值 vs 试验实测值对比（需求4.5第一步，修正系数待数据积累）
    auto *learnBtn = new RibbonButton(QStringLiteral("自学习对比"), ":/icons/ai_assist.svg", gMem);
    learnBtn->setCheckable(false);
    connect(learnBtn, &QToolButton::clicked, this, [this]() {
        SelfLearnDialog dlg(this);
        dlg.exec();
    });
    gMem->addButton(learnBtn);
    m_optimizeRibbon->addSeparator();

    auto *g2 = m_optimizeRibbon->addGroup(QStringLiteral("寻优计算"));
    auto *startBtn = new RibbonButton(QStringLiteral("开始运行计算"), ":/icons/play.svg", g2);
    startBtn->setCheckable(false);
    connect(startBtn, &QToolButton::clicked, this, &EnterCalcPage::onOptimizeStart);
    g2->addButton(startBtn);
    m_pauseBtn = new RibbonButton(QStringLiteral("暂停计算"), ":/icons/pause.svg", g2);
    m_pauseBtn->setCheckable(false);
    connect(m_pauseBtn, &QToolButton::clicked, this, &EnterCalcPage::onOptimizePause);
    g2->addButton(m_pauseBtn);
    auto *stopBtn = new RibbonButton(QStringLiteral("停止计算"), ":/icons/stop.svg", g2);
    stopBtn->setCheckable(false);
    connect(stopBtn, &QToolButton::clicked, this, &EnterCalcPage::onOptimizeStop);
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
    // 筛选单独一列（图标在上、文字在下）
    auto *filterBtn = new RibbonButton(QStringLiteral("筛选"), ":/icons/filter.svg", g2);
    filterBtn->setCheckable(false);
    connect(filterBtn, &QToolButton::clicked, this, &EnterCalcPage::onFilterSchemes);
    g2->addButton(filterBtn);
    // 高级列：高级/选择/取消
    auto *filterCol = new QVBoxLayout();
    filterCol->setSpacing(1);
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
    // 切换、查找替换各自单独一列（图标在上、文字在下）
    auto *switchBtn = new RibbonButton(QStringLiteral("切换"), ":/icons/switch_az.svg", g2);
    switchBtn->setCheckable(false);
    connect(switchBtn, &QToolButton::clicked, this, &EnterCalcPage::onSwitchSortColumn);
    g2->addButton(switchBtn);
    auto *findBtn = new RibbonButton(QStringLiteral("查找替换"), ":/icons/find_replace.svg", g2);
    findBtn->setCheckable(false);
    connect(findBtn, &QToolButton::clicked, this, &EnterCalcPage::onFindScheme);
    g2->addButton(findBtn);
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
    // AI 对比：方案表前若干方案汇总指标 → LLM 对比评价（阶段三）
    auto *aiCompareBtn = new RibbonButton(QStringLiteral("AI 对比"), ":/icons/memory_on.svg", g4);
    aiCompareBtn->setCheckable(false);
    connect(aiCompareBtn, &QToolButton::clicked, this, [this]() {
        // 取方案表前 5 个方案的汇总指标（方案序号 + 引擎输出）
        if (m_schemeData.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("暂无方案"),
                QStringLiteral("请先执行计算或寻优生成方案，再使用 AI 对比"));
            return;
        }
        QList<int> idxs = m_schemeData.keys();
        std::sort(idxs.begin(), idxs.end());
        QString data = QStringLiteral("候选方案对比（均为引擎计算结果）：\n");
        int count = 0;
        for (int idx : idxs) {
            if (count++ >= 5) break;   // 最多 5 个，控制 token
            const auto &c = m_schemeData.value(idx);
            const auto &r = c.result;
            data += QStringLiteral(
                "方案%1：主材成本 %2 元；空载损耗 %3 W；负载损耗 %4 W；"
                "阻抗电压 %5%；心柱磁密 %6 T；油面温升 %7 K；"
                "铁芯直径 %8 mm；低压匝数 %9\n")
                .arg(idx)
                .arg(c.scheme.costCuFeOil, 0, 'f', 0)
                .arg(r.core.noLoadLoss_W, 0, 'f', 0)
                .arg(r.winding.loadLoss_W, 0, 'f', 0)
                .arg(r.impedance.impedance_pct, 0, 'f', 2)
                .arg(r.core.fluxDensity_core_T, 0, 'f', 3)
                .arg(r.thermal.oilRise_K, 0, 'f', 1)
                .arg(c.input.coreDiameter_mm, 0, 'f', 0)
                .arg(c.input.lvTurns);
        }
        AiAnalysisDialog dlg(QStringLiteral("AI 方案对比"),
            QStringLiteral("任务：对比以上候选方案，从成本、损耗性能、温升等维度"
                           "评价各方案优劣，给出推荐排序和理由。"),
            data, this);
        dlg.exec();
    });
    g4->addButton(aiCompareBtn);
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

    // ---- 组6 方案存储：批量保存/打开方案库（JSON 文件持久化）----
    auto *g6 = m_schemeRibbon->addGroup(QStringLiteral("方案存储"));
    auto *saveLibBtn = new RibbonButton(QStringLiteral("保存方案库"), ":/icons/save_scheme.svg", g6);
    saveLibBtn->setCheckable(false);
    connect(saveLibBtn, &QToolButton::clicked, this, &EnterCalcPage::onSaveSchemes);
    g6->addButton(saveLibBtn);
    auto *openLibBtn = new RibbonButton(QStringLiteral("打开方案库"), ":/icons/library.svg", g6);
    openLibBtn->setCheckable(false);
    connect(openLibBtn, &QToolButton::clicked, this, &EnterCalcPage::onLoadSchemes);
    g6->addButton(openLibBtn);
}

// 输出打印 Ribbon：与原型图两组布局一致
// 打印（设置/快速/打印/预览/报价单/计算单/三张参数表）| 输出Excel
void EnterCalcPage::buildPrintRibbon()
{
    auto *g1 = m_printRibbon->addGroup(QStringLiteral("打印"));
    auto *setupBtn = new RibbonButton(QStringLiteral("打印设置"), ":/icons/print_setup.svg", g1);
    setupBtn->setCheckable(false);
    connect(setupBtn, &QToolButton::clicked, this, &EnterCalcPage::onPrintSetup);
    g1->addButton(setupBtn);
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
    auto *quoteBtn = new RibbonButton(QStringLiteral("打开报价单"), ":/icons/quote_open.svg", g1);
    quoteBtn->setCheckable(false);
    connect(quoteBtn, &QToolButton::clicked, this, &EnterCalcPage::onOpenQuote);
    g1->addButton(quoteBtn);
    auto *sheetBtn = new RibbonButton(QStringLiteral("打开计算单"), ":/icons/calc_sheet.svg", g1);
    sheetBtn->setCheckable(false);
    connect(sheetBtn, &QToolButton::clicked, this, &EnterCalcPage::onOpenCalcSheet);
    g1->addButton(sheetBtn);
    // 三张参数表：紧凑堆叠一列
    const auto compact = [](RibbonButton *b) {
        b->setIconSize(QSize(16, 16));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setMinimumSize(150, 20);
        b->setMaximumHeight(20);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        b->setStyleSheet("QToolButton { padding: 0px 4px; }");
    };
    auto *tablesCol = new QVBoxLayout();
    tablesCol->setSpacing(1);
    auto *t1 = new RibbonButton(QStringLiteral("打开绝缘半径表"), ":/icons/table_open.svg", g1);
    compact(t1);
    t1->setCheckable(false);
    connect(t1, &QToolButton::clicked, this, &EnterCalcPage::onOpenInsulRadiusTable);
    tablesCol->addWidget(t1);
    auto *t2 = new RibbonButton(QStringLiteral("打开铁芯尺寸表"), ":/icons/table_open.svg", g1);
    compact(t2);
    t2->setCheckable(false);
    connect(t2, &QToolButton::clicked, this, &EnterCalcPage::onOpenCoreSizeTable);
    tablesCol->addWidget(t2);
    auto *t3 = new RibbonButton(QStringLiteral("打开性能参数比对表"), ":/icons/table_open.svg", g1);
    compact(t3);
    t3->setCheckable(false);
    connect(t3, &QToolButton::clicked, this, &EnterCalcPage::onOpenPerfCompareTable);
    tablesCol->addWidget(t3);
    g1->contentLayout()->addLayout(tablesCol);
    m_printRibbon->addSeparator();

    auto *g2 = m_printRibbon->addGroup(QStringLiteral("输出Excel"));
    auto *docBtn = new RibbonButton(QStringLiteral("导出计算单与成本清单"), ":/icons/excel_export.svg", g2);
    docBtn->setCheckable(false);
    connect(docBtn, &QToolButton::clicked, this, &EnterCalcPage::onExportDocuments);
    g2->addButton(docBtn);
    auto *b4 = new RibbonButton(QStringLiteral("输出外部文件"), ":/icons/excel_export.svg", g2);
    b4->setCheckable(false);
    connect(b4, &QToolButton::clicked, this, &EnterCalcPage::onExportCsv);
    g2->addButton(b4);
    auto *stackBtn = new RibbonButton(QStringLiteral("叠铁铁芯片下料表"), ":/icons/excel_stack.svg", g2);
    stackBtn->setCheckable(false);
    connect(stackBtn, &QToolButton::clicked, this, &EnterCalcPage::onExportStackTable);
    g2->addButton(stackBtn);
    auto *cfgBtn = new RibbonButton(QStringLiteral("计算单配置关联"), ":/icons/excel_star.svg", g2);
    cfgBtn->setCheckable(false);
    connect(cfgBtn, &QToolButton::clicked, this, &EnterCalcPage::onCalcSheetConfig);
    g2->addButton(cfgBtn);
    auto *swBtn = new RibbonButton(QStringLiteral("保存为软件计算单"), ":/icons/excel_save.svg", g2);
    swBtn->setCheckable(false);
    connect(swBtn, &QToolButton::clicked, this, &EnterCalcPage::onSaveSoftwareSheet);
    g2->addButton(swBtn);
    m_printRibbon->addSeparator();
    auto *customBtn = new RibbonButton(QStringLiteral("保存为自定义计算单"), ":/icons/excel_custom.svg", g2);
    customBtn->setCheckable(false);
    connect(customBtn, &QToolButton::clicked, this, &EnterCalcPage::onSaveCustomSheet);
    g2->addButton(customBtn);
    m_printRibbon->addSeparator();

    // ---- AI 说明草稿：基于引擎输出生成计算书文字说明（阶段三）----
    auto *g3 = m_printRibbon->addGroup(QStringLiteral("AI 辅助"));
    auto *aiDraftBtn = new RibbonButton(QStringLiteral("AI 说明草稿"), ":/icons/memory_on.svg", g3);
    aiDraftBtn->setCheckable(false);
    connect(aiDraftBtn, &QToolButton::clicked, this, [this]() {
        if (!m_hasResult) {
            onRunEmCalc();
            if (!m_hasResult) {
                return;
            }
        }
        // 打印表数据 → 文本（左/右双栏行合并为键值对）
        const PrintOutputData po =
            ElectromagneticEngine::buildPrintOutput(m_calcInput, m_emResult);
        QString data = QStringLiteral("计算单数据（引擎输出，双栏行已合并）：\n");
        for (const auto &r : po.rows) {
            if (r.isSectionHeader) {
                data += QStringLiteral("【%1】\n").arg(r.leftName);
                continue;
            }
            QString line;
            if (!r.leftName.isEmpty())
                line += QStringLiteral("%1 = %2 %3").arg(
                    r.leftName, r.leftValue, r.leftUnit).trimmed() + QStringLiteral("；");
            if (!r.rightName.isEmpty())
                line += QStringLiteral("%1 = %2 %3").arg(
                    r.rightName, r.rightValue, r.rightUnit).trimmed();
            if (!line.isEmpty())
                data += line + QStringLiteral("\n");
        }
        AiAnalysisDialog dlg(QStringLiteral("AI 计算书说明草稿"),
            QStringLiteral("任务：为该变压器计算单起草一段文字说明（计算书用），"
                           "包括产品概况、主要设计参数选取、性能指标达标情况和结构特点描述。"
                           "只使用数据中出现的数值，不得编造。"),
            data, this);
        dlg.exec();
    });
    g3->addButton(aiDraftBtn);
}

// 初始化设置组只读查看弹窗：6 个按钮分别展示各项配置当前值；
// 数值来自参数设置页（m_params/m_config/m_calcInput）与寻优默认设置
void EnterCalcPage::showInitInfoDialog(int index)
{
    // 生成「键: 值」只读表格弹窗
    const auto makeDialog = [this](const QString &title,
                                   const QVector<QPair<QString, QString>> &rows) {
        auto *dlg = new QDialog(this);
        dlg->setWindowTitle(title);
        dlg->setModal(true);
        dlg->resize(420, 360);
        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(12, 12, 12, 12);
        auto *table = new QTableWidget(rows.size(), 2, dlg);
        table->setHorizontalHeaderLabels({QStringLiteral("配置项"), QStringLiteral("当前值")});
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setColumnWidth(0, 160);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setAlternatingRowColors(true);
        for (int i = 0; i < rows.size(); ++i) {
            table->setItem(i, 0, new QTableWidgetItem(rows[i].first));
            table->setItem(i, 1, new QTableWidgetItem(rows[i].second));
        }
        layout->addWidget(table, 1);
        auto *closeBtn = new QPushButton(QStringLiteral("关闭"), dlg);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(
            "QPushButton { background: #00bcd4; color: #1a1d23; font-size: 12px;"
            " padding: 6px 20px; border: none; border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background: #4dd0e1; }");
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        layout->addWidget(closeBtn, 0, Qt::AlignCenter);
        dlg->exec();
        dlg->deleteLater();
    };

    // 结构配置枚举值转中文标签
    const auto coreTypeStr = [this]() {
        switch (m_config.coreType) {
        case StructureConfig::StackedSilicon:   return QStringLiteral("叠铁芯");
        case StructureConfig::StereoscopicRoll: return QStringLiteral("立体卷铁芯");
        case StructureConfig::PlanarAmorphous:  return QStringLiteral("平面非晶合金");
        }
        return QStringLiteral("未知");
    };
    const auto coreShapeStr = [this]() {
        switch (m_config.coreShape) {
        case StructureConfig::Circle:     return QStringLiteral("圆形");
        case StructureConfig::LongRound:  return QStringLiteral("长圆形");
        case StructureConfig::Ellipse:    return QStringLiteral("椭圆形");
        case StructureConfig::HalfEllipse: return QStringLiteral("半椭圆形");
        case StructureConfig::EllipseLike: return QStringLiteral("类椭圆型");
        }
        return QStringLiteral("未知");
    };
    const auto windingStr = [this]() {
        return m_config.windingForm == StructureConfig::DualSplit
                   ? QStringLiteral("双分裂") : QStringLiteral("双绕组");
    };
    const auto hvCoilStr = [this]() {
        return m_config.hvCoilStructure == StructureConfig::TwoSegCylinder
                   ? QStringLiteral("两段圆筒式") : QStringLiteral("多层圆筒式");
    };

    switch (index) {
    case 0: {  // 产品结构（参数设置页 Ribbon 选型结果）
        makeDialog(QStringLiteral("产品结构 - 当前配置"), {
            { QStringLiteral("变压器大类"),
              m_config.category == StructureConfig::OilImmersed
                  ? QStringLiteral("油浸式") : QStringLiteral("干式") },
            { QStringLiteral("绕组工艺"),
              m_config.windingProcess == StructureConfig::FoilWound
                  ? QStringLiteral("箔绕") : QStringLiteral("线绕") },
            { QStringLiteral("铁芯结构"), coreTypeStr() },
            { QStringLiteral("铁芯截面形状"), coreShapeStr() },
            { QStringLiteral("绕组方式"), windingStr() },
            { QStringLiteral("高压线圈结构"), hvCoilStr() },
            { QStringLiteral("计算模式"),
              m_config.calcMode == StructureConfig::Professional
                  ? QStringLiteral("专业模式") : QStringLiteral("正常模式") },
        });
        break;
    }
    case 1: {  // 材料选择（设计变量中的材料项，单价见系统设置页）
        makeDialog(QStringLiteral("材料选择 - 当前配置"), {
            { QStringLiteral("硅钢片牌号"), m_calcInput.steelGrade },
            { QStringLiteral("硅钢片厚度(mm)"),
              QString::number(m_calcInput.steelThickness_mm) },
            { QStringLiteral("低压线圈材料"), QStringLiteral("铜（箔绕）") },
            { QStringLiteral("高压线圈材料"), QStringLiteral("铜") },
            { QStringLiteral("材料单价"),
              QStringLiteral("见系统设置页「报价参数」") },
        });
        break;
    }
    case 2: {  // 修正参数（寻优出发点，参数设置页表格编辑值）
        makeDialog(QStringLiteral("修正参数 - 当前设计变量基准"), {
            { QStringLiteral("铁芯直径(mm)"),
              QString::number(m_calcInput.coreDiameter_mm) },
            { QStringLiteral("直线段长(mm)"),
              QString::number(m_calcInput.coreStraight_mm) },
            { QStringLiteral("低压匝数"), QString::number(m_calcInput.lvTurns) },
            { QStringLiteral("高压每层匝数"),
              QString::number(m_calcInput.hvTurnsPerLayer) },
            { QStringLiteral("主空道宽(mm)"),
              QString::number(m_calcInput.mainDuctWidth_mm) },
            { QStringLiteral("叠片系数"), QString::number(m_calcInput.stackFactor) },
            { QStringLiteral("修改入口"), QStringLiteral("返回参数设置页编辑") },
        });
        break;
    }
    case 3: {  // 循环参数（寻优引擎默认设置，暂不可配置）
        makeDialog(QStringLiteral("循环参数 - 寻优计算设置"), {
            { QStringLiteral("寻优方式"), QStringLiteral("网格搜索") },
            { QStringLiteral("搜索维度"),
              QStringLiteral("直径/直线段/低压匝数/高压每层匝数") },
            { QStringLiteral("线程数"), QStringLiteral("4") },
            { QStringLiteral("成本模型"), QStringLiteral("铜+铁+油") },
            { QStringLiteral("备注"), QStringLiteral("围绕当前设计变量基准搜索") },
        });
        break;
    }
    case 4: {  // 约束条件（性能指标标准值与允许偏差）
        makeDialog(QStringLiteral("约束条件 - 性能指标约束"), {
            { QStringLiteral("空载损耗标准(W)"),
              QString::number(m_params.noLoadLossStd_W) },
            { QStringLiteral("空载损耗允许偏差(%)"),
              QString::number(m_params.noLoadLossMaxDev_pct) },
            { QStringLiteral("负载损耗标准(W)"),
              QString::number(m_params.loadLossStd_W) },
            { QStringLiteral("负载损耗允许偏差(%)"),
              QString::number(m_params.loadLossMaxDev_pct) },
            { QStringLiteral("总损耗标准(W)"),
              QString::number(m_params.totalLossStd_W) },
            { QStringLiteral("总损耗允许偏差(%)"),
              QString::number(m_params.totalLossMaxDev_pct) },
            { QStringLiteral("阻抗电压标准(%)"),
              QString::number(m_params.impedanceVoltageStd_pct) },
            { QStringLiteral("阻抗电压允许偏差(%)"),
              QStringLiteral("-%1 / +%2")
                  .arg(QString::number(m_params.impedanceVoltageMinDev_pct),
                       QString::number(m_params.impedanceVoltageMaxDev_pct)) },
            { QStringLiteral("空载电流标准(%)"),
              QString::number(m_params.noLoadCurrentStd_pct) },
            { QStringLiteral("空载电流允许偏差(%)"),
              QString::number(m_params.noLoadCurrentMaxDev_pct) },
            { QStringLiteral("温升限值(K)"),
              QStringLiteral("油面 %1 / 高压 %2 / 低压 %3")
                  .arg(QString::number(m_params.oilTopTempRise_K),
                       QString::number(m_params.hvCoilTempRise_K),
                       QString::number(m_params.lvCoilTempRise_K)) },
        });
        break;
    }
    case 5: {  // 初始化信息（汇总以上各项，寻优前核对）
        makeDialog(QStringLiteral("初始化信息 - 汇总确认"), {
            { QStringLiteral("产品结构"),
              coreTypeStr() + QStringLiteral(" / ") + coreShapeStr() +
                  QStringLiteral(" / ") + windingStr() },
            { QStringLiteral("材料"), m_calcInput.steelGrade },
            { QStringLiteral("设计变量基准"),
              QStringLiteral("D%1mm / N2=%2")
                  .arg(QString::number(m_calcInput.coreDiameter_mm),
                       QString::number(m_calcInput.lvTurns)) },
            { QStringLiteral("寻优方式"), QStringLiteral("网格搜索（4线程）") },
            { QStringLiteral("约束条件"), QStringLiteral("损耗/阻抗/温升超差剔除") },
            { QStringLiteral("核对结果"), m_hasResult
                  ? QStringLiteral("已有计算结果，可直接寻优或重新核对")
                  : QStringLiteral("建议先运行快速计算验证基准方案") },
        });
        break;
    }
    default:
        break;
    }
}

// 竖排"程序选择"导航按钮（点击返回主界面），三个 Tab 各自调用创建
QPushButton *EnterCalcPage::createNavButton(QWidget *parent)
{
    auto *btn = new QPushButton(QStringLiteral("程\n序\n选\n择"), parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setToolTip(QStringLiteral("返回主界面"));
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    btn->setStyleSheet(
        "QPushButton { background: rgba(0,188,212,0.15); color: #4dd0e1;"
        "border: none; border-right: 1px solid #3a4050; border-radius: 0px;"
        "font-size: 12px; padding: 4px; }"
        "QPushButton:hover { background: #00bcd4; color: #0d1117; }");
    connect(btn, &QPushButton::clicked, this, &EnterCalcPage::dashboardRequested);
    return btn;
}

void EnterCalcPage::setupOptimizeTab()
{
    auto *tab = new QWidget();
    auto *layout = new QHBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 程序选择导航按钮，顶格放置于侧边栏左侧
    layout->addWidget(createNavButton(tab));

    // Left sidebar
    auto *sidebar = new SidebarPanel(tab);
    sidebar->addButton(QStringLiteral("选用推荐方案"), ":/icons/recommend.svg");
    sidebar->addButton(QStringLiteral("保存为我的方案"), ":/icons/save_scheme.svg");
    sidebar->addButton(QStringLiteral("从方案库中选择"), ":/icons/library.svg");
    sidebar->addButton(QStringLiteral("返回上一次方案"), ":/icons/undo.svg");
    sidebar->addButton(QStringLiteral("下一步"), ":/icons/enter_calc.svg");
    sidebar->addButton(QStringLiteral("取消"), ":/icons/stop.svg");
    // 前 4 个方案按钮接方案逻辑；取消按钮(index 5)返回优化计算参数设置页
    connect(sidebar, &SidebarPanel::buttonClicked, this, [this](int index) {
        if (index == 5) {
            emit navigateBack();
        } else if (index < 4) {
            onSchemeButtonClicked(index);
        }
    });
    layout->addWidget(sidebar);

    // Structure config table
    auto *table = new QTableWidget(tab);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"#", "结构名称", "选项", "备注"});
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 44);   // 序号列：容纳两位数序号完整显示
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
    auto *outer = new QHBoxLayout(tab);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    // 程序选择导航按钮（返回主界面）
    outer->addWidget(createNavButton(tab));

    m_schemeTable = new SchemeTableWidget(tab);
    // 行内「选择」按钮：标记待确认方案（按钮变亮，不跳转）
    connect(m_schemeTable, &SchemeTableWidget::schemeSelected,
            this, &EnterCalcPage::onSchemeSelected);
    outer->addWidget(m_schemeTable, 1);
    m_stack->addWidget(tab);
}

void EnterCalcPage::setupPrintTab()
{
    auto *tab = new QWidget();
    auto *outer = new QHBoxLayout(tab);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    // 程序选择导航按钮（返回主界面）
    outer->addWidget(createNavButton(tab));

    m_printTable = new PrintTableWidget(tab);
    // 初始计算单延迟到 setCalcInput（进入页面同步设计变量时计算），
    // 构造时 m_calcInput 尚未同步，预计算会显示默认方案结果
    outer->addWidget(m_printTable, 1);
    m_stack->addWidget(tab);
}

void EnterCalcPage::setCalcInput(const CalcInput &input)
{
    m_calcInput = input;
    // 进入计算页时刷新打印表初始计算单（当前设计变量的计算结果；
    // 快速计算/寻优完成后会由 onRunEmCalc 等用最新结果覆盖）
    if (!m_printTable) {
        return;   // 构造期间 Tab 未建完（正常流程不会发生）
    }
    CalcResult initResult;
    if (m_engine.calcElectromagnetic(m_calcInput, initResult) && initResult.valid) {
        m_printTable->loadData(
            ElectromagneticEngine::buildPrintOutput(m_calcInput, initResult));
    }
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
    CalcInput input = m_calcInput;   // 参数设置页编辑的设计变量（默认 SB20-M-630-10）
    m_lastInput = input;
    if (!m_engine.calcElectromagnetic(input, m_emResult) || !m_emResult.valid) {
        m_statusBar->setText(QStringLiteral("电磁计算失败: %1").arg(m_emResult.error));
        QMessageBox::warning(this, QStringLiteral("电磁计算"),
                             QStringLiteral("计算失败: %1").arg(m_emResult.error));
        return;
    }
    m_hasResult = true;

    // 结果面板 + 打印表 + 方案入库
    m_emResultPanel->loadResult(m_emResult);
    m_printTable->loadData(ElectromagneticEngine::buildPrintOutput(input, m_emResult));
    appendScheme(input, m_emResult);

    QString status = QStringLiteral(
        "电磁计算完成：空载损耗 %1 W | 负载损耗 %2 W | 阻抗电压 %3% | "
        "油面温升 %4 K | 总重 %5 kg | 材料成本 %6 元")
        .arg(QString::number(m_emResult.core.noLoadLoss_W, 'f', 0),
             QString::number(m_emResult.winding.loadLoss_W, 'f', 0),
             QString::number(m_emResult.impedance.impedance_pct, 'f', 2),
             QString::number(m_emResult.thermal.oilRise_K, 'f', 1),
             QString::number(m_emResult.mass.totalWeight_kg, 'f', 0),
             QString::number(m_emResult.cost.materialCost, 'f', 0));

    // 约束校验：超差项追加提示（快速计算结果仍展示，仅提示不拦截）
    const SchemeConstraintsResult check = checkSchemeConstraints(m_params, m_emResult);
    if (!check.passed) {
        status += QStringLiteral(" ｜ 注意：%1").arg(check.violations.join(QStringLiteral("，")));
    }
    m_statusBar->setText(status);
}

// 侧边栏方案按钮：应用方案到 m_calcInput 后立即重算（结果面板/打印表/方案表联动刷新）
void EnterCalcPage::onSchemeButtonClicked(int index)
{
    switch (index) {
    case 0: {  // 选用推荐方案（内置推荐表，只读无删除）
        QVector<SchemeStore::SchemeEntry> entries;
        for (const auto &s : RecommendSchemes::all()) {
            SchemeStore::SchemeEntry e;
            e.name = s.name;
            e.input = s.input;
            entries.append(e);
        }
        SchemePickDialog dlg(QStringLiteral("选用推荐方案"), entries, QString(), this);
        if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
            m_calcInput = dlg.selectedEntry().input;
            onRunEmCalc();
        }
        break;
    }
    case 1: {  // 保存为我的方案（命名保存当前设计变量，同名提示覆盖）
        bool ok = false;
        const QString name = QInputDialog::getText(this,
            QStringLiteral("保存为我的方案"),
            QStringLiteral("方案名称："), QLineEdit::Normal,
            QStringLiteral("我的方案1"), &ok);
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }
        QVector<SchemeStore::SchemeEntry> entries =
            SchemeStore::loadEntries(SchemeStore::mySchemesPath());
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].name == name.trimmed()) {
                if (QMessageBox::question(this, QStringLiteral("方案已存在"),
                        QStringLiteral("方案库中已有同名方案「%1」，是否覆盖？").arg(name.trimmed()))
                        != QMessageBox::Yes) {
                    return;
                }
                entries.removeAt(i);
                break;
            }
        }
        SchemeStore::SchemeEntry e;
        e.name = name.trimmed();
        e.savedAt = QDateTime::currentDateTime();
        e.input = m_calcInput;
        entries.prepend(e);
        if (SchemeStore::saveEntries(SchemeStore::mySchemesPath(), entries)) {
            QMessageBox::information(this, QStringLiteral("保存成功"),
                QStringLiteral("方案「%1」已保存到我的方案库").arg(e.name));
        } else {
            QMessageBox::warning(this, QStringLiteral("保存失败"),
                QStringLiteral("无法写入方案库文件"));
        }
        break;
    }
    case 2: {  // 从方案库中选择（我的方案库，支持删除所选）
        QVector<SchemeStore::SchemeEntry> entries =
            SchemeStore::loadEntries(SchemeStore::mySchemesPath());
        if (entries.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("方案库为空"),
                QStringLiteral("暂无已保存方案，请先用「保存为我的方案」添加"));
            return;
        }
        SchemePickDialog dlg(QStringLiteral("从方案库中选择"), entries,
                             SchemeStore::mySchemesPath(), this);
        if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
            m_calcInput = dlg.selectedEntry().input;
            onRunEmCalc();
        }
        break;
    }
    case 3: {  // 返回上一次方案（最近一次实际计算的输入）
        if (!m_hasResult) {
            QMessageBox::information(this, QStringLiteral("暂无记录"),
                QStringLiteral("还没有计算记录，请先执行计算"));
            return;
        }
        m_calcInput = m_lastInput;
        onRunEmCalc();
        break;
    }
    default:
        break;
    }
}

// ---- 异步寻优 ----

void EnterCalcPage::onOptimizeStart()
{
    if (m_optRunning) {
        m_statusBar->setText(QStringLiteral("寻优正在进行中"));
        return;
    }
    // 新一轮寻优：清空方案表与方案数据缓存，从当前设计变量（参数设置页传入）出发网格搜索
    m_schemeTable->clearResults();
    m_schemeData.clear();
    m_confirmedSchemeIdx = -1;
    m_optRunning = true;
    if (m_pauseBtn) {
        m_pauseBtn->setText(QStringLiteral("暂停计算"));
    }
    OptimizationSettings settings;
    m_optimizer->start(m_params, m_config, m_calcInput, settings);
    m_statusBar->setText(
        QStringLiteral("寻优已启动：围绕当前设计变量网格搜索（直径/直线段/低压匝数/高压每层匝数），"
                       "损耗/阻抗/温升超差方案自动剔除"));
}

void EnterCalcPage::onOptimizePause()
{
    if (!m_optRunning || !m_pauseBtn) {
        return;
    }
    const bool paused = (m_pauseBtn->text() == QStringLiteral("暂停计算"));
    if (paused) {
        m_optimizer->pause();
        m_pauseBtn->setText(QStringLiteral("继续计算"));
        m_statusBar->setText(QStringLiteral("寻优已暂停，点击\"继续计算\"恢复"));
    } else {
        m_optimizer->resume();
        m_pauseBtn->setText(QStringLiteral("暂停计算"));
        m_statusBar->setText(QStringLiteral("寻优已恢复"));
    }
}

void EnterCalcPage::onOptimizeStop()
{
    if (!m_optRunning) {
        m_statusBar->setText(QStringLiteral("当前没有进行中的寻优"));
        return;
    }
    m_optimizer->stop();
    m_statusBar->setText(QStringLiteral("正在停止寻优…"));
}

void EnterCalcPage::onOptimizeProgress(int percent)
{
    m_statusBar->setText(QStringLiteral("寻优进行中：%1%").arg(percent));
}

void EnterCalcPage::onOptimizeCandidate(const OptimizeCandidate &candidate)
{
    OptimizationResult scheme = candidate.scheme;
    scheme.schemeIdx = m_schemeTable->rowCount() + 1;   // 序号按入库顺序编排
    m_schemeTable->addResult(scheme);
    // 保存完整候选（input+result+方案行），供确认方案时取回
    OptimizeCandidate saved = candidate;
    saved.scheme = scheme;
    m_schemeData.insert(scheme.schemeIdx, saved);
}

void EnterCalcPage::onOptimizeFinished(bool stopped, const OptimizeCandidate &best,
                                       int total, int valid)
{
    m_optRunning = false;
    if (m_pauseBtn) {
        m_pauseBtn->setText(QStringLiteral("暂停计算"));
    }
    if (valid > 0) {
        // 最优（材料成本最低）方案加载到结果面板与打印/保存链路
        m_lastInput = best.input;
        m_emResult = best.result;
        m_hasResult = true;
        m_emResultPanel->loadResult(m_emResult);
        m_printTable->loadData(ElectromagneticEngine::buildPrintOutput(best.input, best.result));
    }
    const int rejected = total - valid;   // 计算失败或约束超差被剔除的组合数
    if (stopped) {
        m_statusBar->setText(QStringLiteral("寻优已停止：评估 %1 个组合，%2 个通过约束，"
                                            "%3 个剔除").arg(total).arg(valid).arg(rejected));
    } else if (valid > 0) {
        m_statusBar->setText(
            QStringLiteral("寻优完成：评估 %1 个组合，%2 个通过约束（剔除 %3 个），"
                           "最优材料成本 %4 元")
                .arg(total).arg(valid).arg(rejected)
                .arg(QString::number(best.result.cost.materialCost, 'f', 0)));
    } else {
        m_statusBar->setText(QStringLiteral(
            "寻优完成：评估 %1 个组合，全部被约束剔除——请在参数设置页核对"
            "性能标准值与允许偏差").arg(total));
    }
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

// 表格分页绘制到打印机（表头每页重复，列宽按打印区宽度等比）；
// 按页面物理尺寸设置打印字号（HighResolution 打印机 DPI 远高于屏幕，
// 直接用屏幕字号会导致表格在纸面上只占一角）
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

    // 打印专用字号：point 为物理单位，与打印机分辨率解耦；
    // 度量必须以打印机设备为基准（QFontMetricsF 默认按屏幕 DPI 换算，
    // 在 HighResolution 打印画布上行高会缩成十几个设备像素，整表挤压）
    const QFont bodyFont(QStringLiteral("Microsoft YaHei"), 9);
    QFont bFont = bodyFont;
    bFont.setBold(true);
    const QFontMetricsF fmBody(bodyFont, printer);
    const QFontMetricsF fmHead(bFont, printer);
    const double rowH = fmBody.height() * 1.4;
    const double headH = fmHead.height() * 1.4;

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
    // 优先确认行内「选择」按钮标记的方案；无标记时退回当前选中行
    int row = m_schemeTable->markedRow();
    if (row < 0) {
        row = m_schemeTable->currentRow();
    }
    if (row < 0 || m_schemeTable->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("方案确认"),
                                 QStringLiteral("请先在方案表中点击行内\"选择\"按钮标记一个方案"));
        return;
    }
    confirmSchemeAt(row);
}

void EnterCalcPage::onSchemeSelected(int row)
{
    if (row < 0 || row >= m_schemeTable->rowCount()) {
        return;
    }
    // 仅标记待确认方案（按钮高亮由表格内部处理），选中该行但不跳转
    m_schemeTable->selectRow(row);
    const QString idx = m_schemeTable->item(row, 1)
                            ? m_schemeTable->item(row, 1)->text() : QString('?');
    m_statusBar->setText(
        QStringLiteral("已选择方案 %1，点击\"方案确认\"进入输出打印").arg(idx));
}

void EnterCalcPage::onSaveSchemes()
{
    if (m_schemeData.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("保存方案库"),
                                 QStringLiteral("当前没有可保存的方案，请先执行快速计算或寻优计算"));
        return;
    }
    // 按方案序号排序收集设计变量（序号即保存顺序）
    QList<int> idxs = m_schemeData.keys();
    std::sort(idxs.begin(), idxs.end());
    QVector<CalcInput> inputs;
    inputs.reserve(idxs.size());
    for (int idx : idxs) {
        inputs.append(m_schemeData.value(idx).input);
    }

    const QString defaultName = QStringLiteral("schemes_%1.json")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存方案库"), defaultName,
        QStringLiteral("方案库文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    if (!SchemeStore::saveSchemes(
            path, inputs,
            QStringLiteral("容量 %1 kVA / 高压 %2 kV / 低压 %3 kV")
                .arg(m_params.capacity_kVA)
                .arg(m_params.hvRatedVoltage_kV)
                .arg(m_params.lvRatedVoltage_kV))) {
        QMessageBox::warning(this, QStringLiteral("保存方案库"),
                             QStringLiteral("保存失败：无法写入文件 %1").arg(path));
        return;
    }
    m_statusBar->setText(QStringLiteral("方案库已保存：%1（%2 个方案）")
                             .arg(QDir::toNativeSeparators(path)).arg(inputs.size()));
}

void EnterCalcPage::onLoadSchemes()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开方案库"), QString(),
        QStringLiteral("方案库文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    bool ok = false;
    const QVector<CalcInput> inputs = SchemeStore::loadSchemes(path, &ok);
    if (!ok || inputs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("打开方案库"),
                             QStringLiteral("加载失败：文件不存在或不是有效的方案库文件"));
        return;
    }
    if (!m_schemeData.isEmpty()
            && QMessageBox::question(
                   this, QStringLiteral("打开方案库"),
                   QStringLiteral("加载将清空当前方案表中的 %1 个方案，是否继续？")
                       .arg(m_schemeData.size()))
                   != QMessageBox::Yes) {
        return;
    }

    // 清空当前方案，逐个重算恢复（引擎确定性，结果与保存时一致）
    m_schemeTable->clearResults();
    m_schemeData.clear();
    m_confirmedSchemeIdx = -1;
    int loaded = 0;
    for (const CalcInput &in : inputs) {
        CalcResult r;
        if (m_engine.calcElectromagnetic(in, r) && r.valid) {
            appendScheme(in, r);
            ++loaded;
        }
    }
    m_statusBar->setText(QStringLiteral("方案库已加载：%1（%2/%3 个方案重算成功）")
                             .arg(QDir::toNativeSeparators(path))
                             .arg(loaded).arg(inputs.size()));
}

void EnterCalcPage::confirmSchemeAt(int row)
{
    const int schemeIdx = m_schemeTable->item(row, 1)
                              ? m_schemeTable->item(row, 1)->text().toInt() : -1;
    m_confirmedSchemeIdx = schemeIdx;
    m_schemeTable->selectRow(row);

    // 把确认方案切换为"当前方案"：结果面板/打印表/报价单/计算单/
    // CSV 导出等后续输出全部读 m_lastInput/m_emResult，随确认联动变化
    const auto it = m_schemeData.constFind(schemeIdx);
    if (it != m_schemeData.constEnd() && it->result.valid) {
        m_lastInput = it->input;
        m_emResult = it->result;
        m_hasResult = true;
        m_emResultPanel->loadResult(m_emResult);
        m_printTable->loadData(ElectromagneticEngine::buildPrintOutput(it->input, it->result));
        // 通知主界面：已确认方案数据同步到产品报价页
        emit schemeConfirmed(m_params, m_lastInput, m_emResult);
    }

    if (m_schemeIndexSpin) {
        m_schemeIndexSpin->blockSignals(true);
        m_schemeIndexSpin->setValue(schemeIdx > 0 ? schemeIdx : 1);
        m_schemeIndexSpin->blockSignals(false);
    }
    m_statusBar->setText(
        QStringLiteral("已确认方案 %1（主材成本 %2 元），输出打印已更新")
            .arg(schemeIdx)
            .arg(QString::number(m_emResult.cost.materialCost, 'f', 0)));
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
            m_schemeTable->scrollToItem(m_schemeTable->item(r, 1));
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
    // 语义：把"当前选中方案"与"已确认方案"（方案库基准）逐参数对比。
    // 基准按方案序号记录（排序/筛选后仍指向同一方案），从 m_schemeData 取完整数据
    const auto itConf = m_schemeData.constFind(m_confirmedSchemeIdx);
    if (m_confirmedSchemeIdx <= 0 || itConf == m_schemeData.constEnd()) {
        QMessageBox::information(this, QStringLiteral("方案库比较"),
                                 QStringLiteral("尚无已确认方案，请先在方案表中点击行内\"选择\"按钮"
                                                 "或选中方案后点\"方案确认\""));
        return;
    }
    const int cur = m_schemeTable->currentRow();
    const int curIdx = (cur >= 0 && m_schemeTable->item(cur, 1))
                           ? m_schemeTable->item(cur, 1)->text().toInt() : -1;
    const auto itCur = m_schemeData.constFind(curIdx);
    if (cur < 0 || curIdx <= 0 || itCur == m_schemeData.constEnd()) {
        QMessageBox::information(this, QStringLiteral("方案库比较"),
                                 QStringLiteral("请先在方案表中选中待比较的方案（不能与已确认方案相同）"));
        return;
    }
    if (curIdx == m_confirmedSchemeIdx) {
        QMessageBox::information(this, QStringLiteral("方案库比较"),
                                 QStringLiteral("当前选中即为已确认方案 %1，请选择其他方案比较")
                                     .arg(curIdx));
        return;
    }

    const OptimizationResult &a = itCur->scheme;    // 当前选中方案
    const OptimizationResult &b = itConf->scheme;   // 已确认方案（基准）
    const auto line = [](const QString &name, double va, double vb, const QString &unit,
                         int prec = 2) {
        return QStringLiteral("%1：%2 %4 ｜ 已确认：%3 %4\n")
            .arg(name)
            .arg(QString::number(va, 'f', prec), QString::number(vb, 'f', prec), unit);
    };
    QString text = QStringLiteral("【待比较】方案 %1  vs  【已确认】方案 %2\n\n")
                       .arg(curIdx).arg(m_confirmedSchemeIdx);
    text += line(QStringLiteral("主材成本（铜铁油）"), a.costCuFeOil, b.costCuFeOil,
                 QStringLiteral("元"), 0);
    text += line(QStringLiteral("铜铁成本"), a.costCuFe, b.costCuFe, QStringLiteral("元"), 0);
    text += line(QStringLiteral("铁芯直径"), a.coreD, b.coreD, QStringLiteral("mm"));
    text += line(QStringLiteral("铁芯长轴"), a.coreL, b.coreL, QStringLiteral("mm"));
    text += line(QStringLiteral("低压匝数"), double(a.lvTurns), double(b.lvTurns), QString(), 0);
    text += line(QStringLiteral("高压线圈层数"), double(a.hvLayers), double(b.hvLayers),
                 QString(), 0);
    text += line(QStringLiteral("主空道尺寸"), a.mainDuct, b.mainDuct, QStringLiteral("mm"));
    const double diff = a.costCuFeOil - b.costCuFeOil;
    text += QStringLiteral("\n结论：待比较方案主材成本比已确认方案%1 %2 元")
                .arg(diff <= 0 ? QStringLiteral("低") : QStringLiteral("高"))
                .arg(QString::number(qAbs(diff), 'f', 0));
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
    m_schemeTable->scrollToItem(m_schemeTable->item(row, 1));
    m_statusBar->setText(QStringLiteral("已选中方案 %1").arg(value));
}

// ---- 输出打印 Tab ----

void EnterCalcPage::onPrintSetup()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("打印设置"));
    auto *form = new QFormLayout(&dlg);
    auto *printerCombo = new QComboBox(&dlg);
    printerCombo->addItem(QStringLiteral("（系统默认打印机）"));
    const auto printers = QPrinterInfo::availablePrinters();
    for (const auto &pi : printers) {
        printerCombo->addItem(pi.printerName());
    }
    if (!m_defaultPrinterName.isEmpty()) {
        const int i = printerCombo->findText(m_defaultPrinterName);
        if (i > 0) {
            printerCombo->setCurrentIndex(i);
        }
    }
    form->addRow(QStringLiteral("默认打印机:"), printerCombo);
    auto *orientCombo = new QComboBox(&dlg);
    orientCombo->addItems({QStringLiteral("纵向"), QStringLiteral("横向")});
    orientCombo->setCurrentIndex(m_landscape ? 1 : 0);
    form->addRow(QStringLiteral("页面方向:"), orientCombo);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    m_defaultPrinterName = printerCombo->currentIndex() == 0
                               ? QString() : printerCombo->currentText();
    m_landscape = orientCombo->currentIndex() == 1;
    m_statusBar->setText(QStringLiteral("打印设置已保存：%1 / %2")
                             .arg(m_defaultPrinterName.isEmpty()
                                      ? QStringLiteral("系统默认打印机") : m_defaultPrinterName,
                                  m_landscape ? QStringLiteral("横向") : QStringLiteral("纵向")));
}

void EnterCalcPage::onQuickPrint()
{
    QPrinter printer(QPrinter::HighResolution);
    if (!m_defaultPrinterName.isEmpty()) {
        printer.setPrinterName(m_defaultPrinterName);
    }
    printer.setPageOrientation(m_landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
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
    if (!m_defaultPrinterName.isEmpty()) {
        printer.setPrinterName(m_defaultPrinterName);
    }
    printer.setPageOrientation(m_landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
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
    if (!m_defaultPrinterName.isEmpty()) {
        printer.setPrinterName(m_defaultPrinterName);
    }
    printer.setPageOrientation(m_landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
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

void EnterCalcPage::onOpenQuote()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const auto &c = m_emResult.cost;
    QString text = QStringLiteral("======== 报价单（材料成本明细） ========\n");
    text += QStringLiteral("硅钢片成本: %1 元\n").arg(QString::number(c.steelCost, 'f', 1));
    text += QStringLiteral("高压导线成本: %1 元\n").arg(QString::number(c.hvWireCost, 'f', 1));
    text += QStringLiteral("低压箔成本: %1 元\n").arg(QString::number(c.lvWireCost, 'f', 1));
    text += QStringLiteral("绝缘油成本: %1 元\n").arg(QString::number(c.oilCost, 'f', 1));
    text += QStringLiteral("油箱成本: %1 元\n").arg(QString::number(c.tankCost, 'f', 1));
    text += QStringLiteral("----------------------------------------\n");
    text += QStringLiteral("材料成本合计: %1 元\n").arg(QString::number(c.materialCost, 'f', 1));
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("报价单"));
    box.setText(text);
    box.setTextFormat(Qt::PlainText);
    box.setFont(QFont(QStringLiteral("Consolas")));
    box.exec();
    m_statusBar->setText(QStringLiteral("报价单已打开"));
}

void EnterCalcPage::onOpenCalcSheet()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("计算单 - SB20-M-630-10"));
    dlg.resize(640, 560);
    auto *layout = new QVBoxLayout(&dlg);
    auto *edit = new QPlainTextEdit(&dlg);
    edit->setReadOnly(true);
    edit->setPlainText(EmResultPanel::resultText(m_emResult));
    edit->setFont(QFont(QStringLiteral("Consolas"), 9));
    layout->addWidget(edit);
    dlg.exec();
    m_statusBar->setText(QStringLiteral("计算单已打开"));
}

void EnterCalcPage::onOpenInsulRadiusTable()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const auto &in = m_lastInput;
    const auto &w = m_emResult.winding;
    const double lvOuter = in.coreDiameter_mm / 2.0 + w.lvRadial_mm;
    const double hvInner = lvOuter + m_emResult.impedance.lambda_mm - w.hvRadial_mm;
    QString text = QStringLiteral("项目\t数值(mm)\n");
    text += QStringLiteral("铁芯半径\t%1\n").arg(QString::number(in.coreDiameter_mm / 2.0, 'f', 1));
    text += QStringLiteral("低压辐向厚\t%1\n").arg(QString::number(w.lvRadial_mm, 'f', 1));
    text += QStringLiteral("低压外半径\t%1\n").arg(QString::number(lvOuter, 'f', 1));
    text += QStringLiteral("主空道（含绝缘）\t%1\n").arg(QString::number(w.mainDuct_mm, 'f', 1));
    text += QStringLiteral("高压内半径\t%1\n").arg(QString::number(hvInner, 'f', 1));
    text += QStringLiteral("高压辐向厚\t%1\n").arg(QString::number(w.hvRadial_mm, 'f', 1));
    text += QStringLiteral("高压外半径\t%1\n").arg(QString::number(hvInner + w.hvRadial_mm, 'f', 1));
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("绝缘半径表"));
    box.setText(text);
    box.setTextFormat(Qt::PlainText);
    box.setFont(QFont(QStringLiteral("Consolas")));
    box.exec();
    m_statusBar->setText(QStringLiteral("绝缘半径表已打开"));
}

void EnterCalcPage::onOpenCoreSizeTable()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const auto &core = m_emResult.core;
    QString text = QStringLiteral("级数\t片宽(mm)\t叠厚(mm)\n");
    const int n = qMin(core.widths_mm.size(), core.stacks_mm.size());
    for (int i = 0; i < n; ++i) {
        text += QStringLiteral("%1\t%2\t%3\n")
                    .arg(i + 1)
                    .arg(QString::number(core.widths_mm[i], 'f', 1),
                         QString::number(core.stacks_mm[i], 'f', 1));
    }
    text += QStringLiteral("----------------------------------------\n");
    text += QStringLiteral("心柱截面: %1 cm²\n").arg(QString::number(core.coreArea_cm2, 'f', 2));
    text += QStringLiteral("铁轭截面: %1 cm²\n").arg(QString::number(core.yokeArea_cm2, 'f', 2));
    text += QStringLiteral("短轴长: %1 mm\n").arg(QString::number(core.minorAxis_mm, 'f', 2));
    text += QStringLiteral("大圆半径: %1 mm\n").arg(QString::number(core.majorRadius_mm, 'f', 2));
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("铁芯尺寸表（叠积）"));
    box.setText(text);
    box.setTextFormat(Qt::PlainText);
    box.setFont(QFont(QStringLiteral("Consolas")));
    box.exec();
    m_statusBar->setText(QStringLiteral("铁芯尺寸表已打开"));
}

void EnterCalcPage::onOpenPerfCompareTable()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const auto &r = m_emResult;
    const auto &p = m_params;
    QString text = QStringLiteral("项目\t标准值\t计算值\t判定\n");
    const auto line = [&text](const QString &name, double std, double calc,
                              double maxDevPct, double minDevPct, const QString &unit) {
        const double dev = std > 0.0 ? (calc - std) / std * 100.0 : 0.0;
        const bool ok = dev <= maxDevPct && dev >= minDevPct;
        text += QStringLiteral("%1\t%2\t%3\t%4\n")
                    .arg(name,
                         QString::number(std, 'f', 1) + unit,
                         QString::number(calc, 'f', 1) + unit,
                         ok ? QStringLiteral("合格")
                            : QStringLiteral("超差(%1%)").arg(QString::number(dev, 'f', 1)));
    };
    line(QStringLiteral("空载损耗(W)"), p.noLoadLossStd_W, r.core.noLoadLoss_W,
         p.noLoadLossMaxDev_pct, -100.0, QString());
    line(QStringLiteral("负载损耗(W)"), p.loadLossStd_W, r.winding.loadLoss_W,
         p.loadLossMaxDev_pct, -100.0, QString());
    line(QStringLiteral("总损耗(W)"), p.totalLossStd_W,
         r.core.noLoadLoss_W + r.winding.loadLoss_W,
         p.totalLossMaxDev_pct, -100.0, QString());
    line(QStringLiteral("阻抗电压(%)"), p.impedanceVoltageStd_pct, r.impedance.impedance_pct,
         p.impedanceVoltageMaxDev_pct, p.impedanceVoltageMinDev_pct, QString());
    line(QStringLiteral("空载电流(%)"), p.noLoadCurrentStd_pct, r.core.noLoadCurrent_pct,
         p.noLoadCurrentMaxDev_pct, -100.0, QString());
    line(QStringLiteral("油顶层温升(K)"), p.oilTopTempRise_K, r.thermal.oilTopRise_K,
         0.0, -100.0, QString());
    line(QStringLiteral("高压绕组温升(K)"), p.hvCoilTempRise_K, r.thermal.hvWindingRise_K,
         0.0, -100.0, QString());
    line(QStringLiteral("低压绕组温升(K)"), p.lvCoilTempRise_K, r.thermal.lvWindingRise_K,
         0.0, -100.0, QString());
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("性能参数比对表"));
    box.setText(text);
    box.setTextFormat(Qt::PlainText);
    box.setFont(QFont(QStringLiteral("Consolas")));
    box.exec();
    m_statusBar->setText(QStringLiteral("性能参数比对表已打开"));
}

void EnterCalcPage::onExportStackTable()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("叠铁铁芯片下料表"),
        QStringLiteral("铁芯片下料表.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("叠铁铁芯片下料表"),
                             QStringLiteral("无法写入文件: %1").arg(path));
        return;
    }
    file.write("\xEF\xBB\xBF");
    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QStringLiteral("级数,片宽(mm),叠厚(mm)\n");
    const auto &core = m_emResult.core;
    const int n = qMin(core.widths_mm.size(), core.stacks_mm.size());
    for (int i = 0; i < n; ++i) {
        ts << i + 1 << ',' << QString::number(core.widths_mm[i], 'f', 1)
           << ',' << QString::number(core.stacks_mm[i], 'f', 1) << '\n';
    }
    file.close();
    m_statusBar->setText(QStringLiteral("叠铁铁芯片下料表已导出: %1").arg(path));
}

void EnterCalcPage::onCalcSheetConfig()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const auto &in = m_lastInput;
    QString text = QStringLiteral("======== 计算单配置关联 ========\n");
    text += QStringLiteral("容量: %1 kVA\n").arg(QString::number(in.capacity_kVA, 'f', 0));
    text += QStringLiteral("高压/低压: %1/%2 kV\n")
                .arg(QString::number(in.hvRated_kV, 'f', 1),
                     QString::number(in.lvRated_kV, 'f', 2));
    text += QStringLiteral("硅钢牌号: %1（厚 %2 mm）\n")
                .arg(in.steelGrade, QString::number(in.steelThickness_mm, 'f', 2));
    text += QStringLiteral("铁芯直径: %1 mm / 直线长: %2 mm\n")
                .arg(QString::number(in.coreDiameter_mm, 'f', 0),
                     QString::number(in.coreStraight_mm, 'f', 0));
    text += QStringLiteral("低压: %1 匝 / 箔 %2×%3 mm\n")
                .arg(in.lvTurns)
                .arg(QString::number(in.lvFoilThick_mm, 'f', 2),
                     QString::number(in.lvFoilWidth_mm, 'f', 0));
    text += QStringLiteral("高压: 裸线 %1×%2 mm / 每层 %3 匝\n")
                .arg(QString::number(in.hvBareThick_mm, 'f', 2),
                     QString::number(in.hvBareWidth_mm, 'f', 2))
                .arg(in.hvTurnsPerLayer);
    text += QStringLiteral("主空道: %1 mm（纸板 %2 mm）\n")
                .arg(QString::number(in.mainDuctWidth_mm, 'f', 1),
                     QString::number(in.mainDuctInsul_mm, 'f', 1));
    text += QStringLiteral("叠片系数: %1 / 工艺系数: %2\n")
                .arg(QString::number(in.stackFactor, 'f', 2),
                     QString::number(in.coreLossCraftCoef, 'f', 2));
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("计算单配置关联"));
    box.setText(text);
    box.setTextFormat(Qt::PlainText);
    box.setFont(QFont(QStringLiteral("Consolas")));
    box.exec();
    m_statusBar->setText(QStringLiteral("计算单配置关联已打开"));
}

void EnterCalcPage::onSaveSoftwareSheet()
{
    onSaveCalcSheet();
}

// 一键导出《计算单》《成本清单》两份文件（需求4.1最终方案导出）：
// 以「图号+型号」为唯一标识命名，保存到用户选择的归档目录
void EnterCalcPage::onExportDocuments()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }

    // ---- 导出对话框：图号 + 型号 + 归档目录 ----
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("导出计算单与成本清单"));
    dlg.setFixedWidth(420);
    auto *form = new QFormLayout(&dlg);
    auto *drawingNoEdit = new QLineEdit(&dlg);
    drawingNoEdit->setPlaceholderText(QStringLiteral("如 1BT.520.0001"));
    auto *modelEdit = new QLineEdit(&dlg);
    // 型号默认取当前方案规格（产品大类-容量/电压等级）
    modelEdit->setText(QStringLiteral("%1-M-%2-%3")
                           .arg(m_params.productModel,
                                QString::number(m_params.capacity_kVA, 'f', 0),
                                QString::number(m_params.hvRatedVoltage_kV, 'f', 0)));
    auto *dirEdit = new QLineEdit(&dlg);
    dirEdit->setReadOnly(true);
    dirEdit->setText(QDir::homePath());
    auto *browseBtn = new QPushButton(QStringLiteral("浏览..."), &dlg);
    auto *dirRow = new QWidget(&dlg);
    auto *dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(4);
    dirLayout->addWidget(dirEdit, 1);
    dirLayout->addWidget(browseBtn);
    form->addRow(QStringLiteral("图号："), drawingNoEdit);
    form->addRow(QStringLiteral("型号："), modelEdit);
    form->addRow(QStringLiteral("归档目录："), dirRow);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(browseBtn, &QPushButton::clicked, this, [this, dirEdit]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择归档目录"), dirEdit->text());
        if (!dir.isEmpty()) {
            dirEdit->setText(dir);
        }
    });
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QString drawingNo = drawingNoEdit->text().trimmed();
    const QString model = modelEdit->text().trimmed();
    if (drawingNo.isEmpty() || model.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("导出计算单与成本清单"),
                             QStringLiteral("图号和型号不能为空"));
        return;
    }

    // ---- 计算单文本：打印表双栏数据逐行输出 ----
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    QString sheetText = QStringLiteral("变压器电磁计算单\n");
    sheetText += QStringLiteral("========================================\n");
    sheetText += QStringLiteral("图号：%1　型号：%2\n").arg(drawingNo, model);
    sheetText += QStringLiteral("导出时间：%1\n").arg(stamp);
    sheetText += QStringLiteral("========================================\n");
    const PrintOutputData printData = ElectromagneticEngine::buildPrintOutput(m_lastInput, m_emResult);
    for (const auto &row : printData.rows) {
        if (row.isSectionHeader) {
            sheetText += QStringLiteral("---- %1 ----\n").arg(row.leftName);
        } else {
            sheetText += QStringLiteral("%1: %2 %3\t%4: %5 %6\n")
                             .arg(row.leftName, row.leftValue, row.leftUnit,
                                  row.rightName, row.rightValue, row.rightUnit);
        }
    }

    // ---- 成本清单文本：QuoteCalculator 明细 + 汇总 ----
    bool paramOk = false;
    const QuoteParams quoteParams =
        QuoteParams::loadFromFile(QuoteCalculator::defaultParamsPath(), &paramOk);
    const QuoteResult quote =
        QuoteCalculator::calculate(m_params, m_emResult, paramOk ? quoteParams : QuoteParams{});
    QString costText = QStringLiteral("材料成本清单\n");
    costText += QStringLiteral("========================================\n");
    costText += QStringLiteral("图号：%1　型号：%2\n").arg(drawingNo, model);
    costText += QStringLiteral("导出时间：%1\n").arg(stamp);
    costText += QStringLiteral("========================================\n");
    costText += QStringLiteral("项目\t数量\t单位\t单价\t金额(元)\n");
    for (const QuoteLine &line : quote.lines) {
        costText += QStringLiteral("%1\t%2\t%3\t%4\t%5\n")
                        .arg(line.name,
                             QString::number(line.quantity, 'f', 2),
                             line.unit,
                             QString::number(line.unitPrice, 'f', 2),
                             QString::number(line.amount, 'f', 1));
    }
    costText += QStringLiteral("----------------------------------------\n");
    costText += QStringLiteral("材料成本小计: %1 元\n").arg(QString::number(quote.materialCost, 'f', 1));
    costText += QStringLiteral("费用小计（外购件+人工+管理）: %1 元\n").arg(QString::number(quote.feeCost, 'f', 1));
    costText += QStringLiteral("成本合计: %1 元\n").arg(QString::number(quote.costTotal, 'f', 1));
    costText += QStringLiteral("利润: %1 元\n").arg(QString::number(quote.profit, 'f', 1));
    costText += QStringLiteral("税额: %1 元\n").arg(QString::number(quote.tax, 'f', 1));
    costText += QStringLiteral("含税出厂报价: %1 元\n").arg(QString::number(quote.quotePrice, 'f', 1));

    // ---- 写入两份文件（图号+型号命名，UTF-8 BOM 便于 Excel 打开） ----
    const QString base = QStringLiteral("%1_%2").arg(drawingNo, model);
    const QString sheetPath = QDir(dirEdit->text()).filePath(base + QStringLiteral("_计算单.txt"));
    const QString costPath = QDir(dirEdit->text()).filePath(base + QStringLiteral("_成本清单.txt"));
    const auto writeFile = [this](const QString &path, const QString &text) -> bool {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("导出计算单与成本清单"),
                                 QStringLiteral("无法写入文件: %1").arg(path));
            return false;
        }
        file.write("\xEF\xBB\xBF");
        QTextStream ts(&file);
        ts.setEncoding(QStringConverter::Utf8);
        ts << text;
        file.close();
        return true;
    };
    if (!writeFile(sheetPath, sheetText) || !writeFile(costPath, costText)) {
        return;
    }
    m_statusBar->setText(QStringLiteral("已导出：%1 与 %2")
                             .arg(QDir::toNativeSeparators(sheetPath),
                                  QDir::toNativeSeparators(costPath)));
    QMessageBox::information(this, QStringLiteral("导出计算单与成本清单"),
        QStringLiteral("导出成功：\n%1\n%2")
            .arg(QDir::toNativeSeparators(sheetPath),
                 QDir::toNativeSeparators(costPath)));
}

void EnterCalcPage::onSaveCustomSheet()
{
    if (!m_hasResult) {
        onRunEmCalc();
        if (!m_hasResult) {
            return;
        }
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存为自定义计算单"),
        QStringLiteral("自定义计算单.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存为自定义计算单"),
                             QStringLiteral("无法写入文件: %1").arg(path));
        return;
    }
    file.write("\xEF\xBB\xBF");
    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QStringLiteral("分组,参数名称,数值,单位\n");
    const auto groups = EmResultPanel::buildGroups(m_emResult);
    for (const auto &group : groups) {
        for (const auto &r : group.second) {
            ts << group.first << ',' << r[0] << ',' << r[1] << ',' << r.value(2) << '\n';
        }
    }
    file.close();
    m_statusBar->setText(QStringLiteral("自定义计算单已保存: %1").arg(path));
}

void EnterCalcPage::appendScheme(const CalcInput &input, const CalcResult &result)
{
    const OptimizationResult scheme = makeScheme(m_schemeTable->rowCount() + 1, input, result);
    m_schemeTable->addResult(scheme);
    // 保存完整候选（input+result+方案行），供确认方案时取回
    OptimizeCandidate saved;
    saved.input = input;
    saved.result = result;
    saved.scheme = scheme;
    m_schemeData.insert(scheme.schemeIdx, saved);
}
