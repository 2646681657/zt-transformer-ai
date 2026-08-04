#include "EnterCalcPage.h"
#include "RibbonBar.h"
#include "RibbonGroup.h"
#include "RibbonButton.h"
#include "SidebarPanel.h"
#include "SchemeTableWidget.h"
#include "PrintTableWidget.h"
#include "MockOptimizer.h"
#include "MockCalcEngine.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>

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

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    setupOptimizeTab();
    setupSchemeTab();
    setupPrintTab();
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &EnterCalcPage::onTabChanged);
    mainLayout->addWidget(m_tabWidget, 1);

    // Status bar
    auto *statusBar = new QLabel(QStringLiteral("就绪"), this);
    statusBar->setFixedHeight(22);
    statusBar->setStyleSheet("background: #22262e; padding-left: 8px; font-size: 11px;"
                             "color: #8a9bb0; border-top: 1px solid #3a4050;");
    mainLayout->addWidget(statusBar);
}

void EnterCalcPage::buildOptimizeRibbon()
{
    auto *g1 = m_optimizeRibbon->addGroup(QStringLiteral("初始化设置"));
    g1->addButton(new RibbonButton(QStringLiteral("快速计算"), ":/icons/fast_calc.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("产品结构"), ":/icons/product.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("材料选择"), ":/icons/material.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("修正参数"), ":/icons/adjust.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("循环参数"), ":/icons/loop_param.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("约束条件"), ":/icons/constraint.svg", g1));
    g1->addButton(new RibbonButton(QStringLiteral("初始化信息"), ":/icons/init_info.svg", g1));
    m_optimizeRibbon->addSeparator();

    auto *g2 = m_optimizeRibbon->addGroup(QStringLiteral("寻优计算"));
    auto *startBtn = new RibbonButton(QStringLiteral("开始运行计算"), ":/icons/play.svg", g2);
    startBtn->setCheckable(false);
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
    g3->addButton(schemeBtn);
}

void EnterCalcPage::buildSchemeRibbon()
{
    auto *g1 = m_schemeRibbon->addGroup(QStringLiteral("显示选项"));
    g1->addButton(new RibbonButton(QStringLiteral("显示主要参数"), ":/icons/visibility.svg", g1));
    m_schemeRibbon->addSeparator();

    auto *g2 = m_schemeRibbon->addGroup(QStringLiteral("排序与筛选"));
    g2->addButton(new RibbonButton(QStringLiteral("升序"), ":/icons/sort_asc.svg", g2));
    g2->addButton(new RibbonButton(QStringLiteral("降序"), ":/icons/sort_desc.svg", g2));
    g2->addButton(new RibbonButton(QStringLiteral("筛选"), ":/icons/filter.svg", g2));
    m_schemeRibbon->addSeparator();

    auto *g3 = m_schemeRibbon->addGroup(QStringLiteral("方案选择"));
    auto *confirmBtn = new RibbonButton(QStringLiteral("方案确认"), ":/icons/confirm.svg", g3);
    confirmBtn->setCheckable(false);
    g3->addButton(confirmBtn);
}

void EnterCalcPage::buildPrintRibbon()
{
    auto *g1 = m_printRibbon->addGroup(QStringLiteral("打印"));
    auto *b1 = new RibbonButton(QStringLiteral("快速打印"), ":/icons/print_fast.svg", g1);
    b1->setCheckable(false);
    g1->addButton(b1);
    auto *b2 = new RibbonButton(QStringLiteral("打印"), ":/icons/print.svg", g1);
    b2->setCheckable(false);
    g1->addButton(b2);
    auto *b3 = new RibbonButton(QStringLiteral("打印预览"), ":/icons/preview.svg", g1);
    b3->setCheckable(false);
    g1->addButton(b3);
    m_printRibbon->addSeparator();

    auto *g2 = m_printRibbon->addGroup(QStringLiteral("输出Excel"));
    auto *b4 = new RibbonButton(QStringLiteral("输出外部文件"), ":/icons/export.svg", g2);
    b4->setCheckable(false);
    g2->addButton(b4);
    auto *b5 = new RibbonButton(QStringLiteral("保存为计算单"), ":/icons/save.svg", g2);
    b5->setCheckable(false);
    g2->addButton(b5);
}

void EnterCalcPage::setupOptimizeTab()
{
    auto *tab = new QWidget();
    auto *layout = new QHBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Left sidebar
    auto *sidebar = new SidebarPanel(tab);
    sidebar->addButton(QStringLiteral("选用推荐方案"), ":/icons/recommend.svg");
    sidebar->addButton(QStringLiteral("保存为我的方案"), ":/icons/save_scheme.svg");
    sidebar->addButton(QStringLiteral("从方案库中选择"), ":/icons/library.svg");
    sidebar->addButton(QStringLiteral("返回上一次方案"), ":/icons/undo.svg");
    sidebar->addButton(QStringLiteral("下一步"), ":/icons/enter_calc.svg");
    sidebar->addButton(QStringLiteral("取消"), ":/icons/stop.svg");
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
    layout->addWidget(table, 1);
    m_tabWidget->addTab(tab, QStringLiteral("优化计算"));
}

void EnterCalcPage::setupSchemeTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    m_schemeTable = new SchemeTableWidget(tab);
    layout->addWidget(m_schemeTable);
    m_tabWidget->addTab(tab, QStringLiteral("方案选择"));
}

void EnterCalcPage::setupPrintTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    m_printTable = new PrintTableWidget(tab);
    MockCalcEngine engine;
    m_printTable->loadData(engine.calculate(m_params, m_config));
    layout->addWidget(m_printTable);
    m_tabWidget->addTab(tab, QStringLiteral("输出打印"));
}

void EnterCalcPage::onTabChanged(int index)
{
    m_optimizeRibbon->setVisible(index == 0);
    m_schemeRibbon->setVisible(index == 1);
    m_printRibbon->setVisible(index == 2);
}

void EnterCalcPage::onStartOptimize()
{
    if (!m_optimizer) {
        m_optimizer = new MockOptimizer(this);
        connect(m_optimizer, &IOptimizer::resultReady, m_schemeTable, &SchemeTableWidget::addResult);
    }
    m_schemeTable->clearResults();
    OptimizationSettings settings;
    m_optimizer->start(m_params, m_config, settings);
}
