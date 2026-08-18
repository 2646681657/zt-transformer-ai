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

void EnterCalcPage::buildSchemeRibbon()
{
    auto *g1 = m_schemeRibbon->addGroup(QStringLiteral("显示选项"));
    auto *colBtn = new RibbonButton(QStringLiteral("显示主要参数"), ":/icons/visibility.svg", g1);
    colBtn->setCheckable(false);
    connect(colBtn, &QToolButton::clicked, this, &EnterCalcPage::onSchemeColumnMenu);
    g1->addButton(colBtn);
    m_schemeRibbon->addSeparator();

    auto *g2 = m_schemeRibbon->addGroup(QStringLiteral("排序与筛选"));
    auto *ascBtn = new RibbonButton(QStringLiteral("升序"), ":/icons/sort_asc.svg", g2);
    ascBtn->setCheckable(false);
    connect(ascBtn, &QToolButton::clicked, this, [this]() { onSortSchemes(true); });
    g2->addButton(ascBtn);
    auto *descBtn = new RibbonButton(QStringLiteral("降序"), ":/icons/sort_desc.svg", g2);
    descBtn->setCheckable(false);
    connect(descBtn, &QToolButton::clicked, this, [this]() { onSortSchemes(false); });
    g2->addButton(descBtn);
    auto *filterBtn = new RibbonButton(QStringLiteral("筛选"), ":/icons/filter.svg", g2);
    filterBtn->setCheckable(false);
    connect(filterBtn, &QToolButton::clicked, this, &EnterCalcPage::onFilterSchemes);
    g2->addButton(filterBtn);
    m_schemeRibbon->addSeparator();

    auto *g3 = m_schemeRibbon->addGroup(QStringLiteral("方案选择"));
    auto *confirmBtn = new RibbonButton(QStringLiteral("方案确认"), ":/icons/confirm.svg", g3);
    confirmBtn->setCheckable(false);
    connect(confirmBtn, &QToolButton::clicked, this, &EnterCalcPage::onConfirmScheme);
    g3->addButton(confirmBtn);
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
    // 列 2 = 主材成本（铜铁油）
    m_schemeTable->sortItems(2, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
    m_statusBar->setText(QStringLiteral("方案已按主材成本%1排序")
                             .arg(ascending ? QStringLiteral("升序") : QStringLiteral("降序")));
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
    const QString idx = m_schemeTable->item(row, 1)
                            ? m_schemeTable->item(row, 1)->text() : QString('?');
    m_statusBar->setText(QStringLiteral("已确认方案 %1，跳转输出打印").arg(idx));
    m_tabBar->setCurrentIndex(2);
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
