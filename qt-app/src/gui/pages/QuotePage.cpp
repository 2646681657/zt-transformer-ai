#include "QuotePage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFile>

namespace {

// 数字转文本：保留最多 2 位小数
QString money(double v)
{
    return QString::number(v, 'f', 2);
}

QString qty(double v)
{
    return QString::number(v, 'f', 1);
}

} // namespace

QuotePage::QuotePage(QWidget *parent)
    : QWidget(parent)
{
    // 启动时加载持久化的报价参数（无文件用默认值）
    bool ok = false;
    m_quoteParams = QuoteParams::loadFromFile(QuoteCalculator::defaultParamsPath(), &ok);
    if (!ok) {
        m_quoteParams = QuoteParams();
    }
    setupUi();
}

void QuotePage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 工具栏：页面名 + 当前方案 + 保存参数/导出按钮
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet("background: #2a2f38; border-bottom: 1px solid #3a4050;");
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(8, 4, 8, 4);
    toolLayout->setSpacing(8);

    auto *pageLabel = new QLabel(QStringLiteral("产品报价"), toolbar);
    pageLabel->setStyleSheet("color: #4dd0e1; font-size: 12px; font-weight: bold;");
    toolLayout->addWidget(pageLabel);

    m_schemeLabel = new QLabel(QStringLiteral("未载入方案"), toolbar);
    m_schemeLabel->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    toolLayout->addWidget(m_schemeLabel);

    toolLayout->addStretch();

    auto *saveBtn = new QPushButton(QStringLiteral("保存参数"), toolbar);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(
        "QPushButton { background: rgba(0,188,212,0.15); color: #4dd0e1; border: 1px solid #00bcd4;"
        " border-radius: 3px; padding: 2px 12px; font-size: 11px; }"
        "QPushButton:hover { background: rgba(0,188,212,0.3); }");
    connect(saveBtn, &QPushButton::clicked, this, &QuotePage::onSaveParams);
    toolLayout->addWidget(saveBtn);

    auto *exportBtn = new QPushButton(QStringLiteral("导出报价单"), toolbar);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet(
        "QPushButton { background: rgba(0,188,212,0.15); color: #4dd0e1; border: 1px solid #00bcd4;"
        " border-radius: 3px; padding: 2px 12px; font-size: 11px; }"
        "QPushButton:hover { background: rgba(0,188,212,0.3); }");
    connect(exportBtn, &QPushButton::clicked, this, &QuotePage::onExportQuote);
    toolLayout->addWidget(exportBtn);

    mainLayout->addWidget(toolbar);

    // 内容区：左参数面板 + 右明细表
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setStyleSheet("QSplitter { background: #1a1d23; }"
                            "QSplitter::handle { background: #3a4050; width: 2px; }");

    // ---- 左：报价参数 ----
    auto *paramWidget = new QWidget(splitter);
    paramWidget->setStyleSheet("background: #1e2228;");
    paramWidget->setMinimumWidth(240);
    auto *paramLayout = new QVBoxLayout(paramWidget);
    paramLayout->setContentsMargins(8, 8, 8, 8);
    paramLayout->setSpacing(8);

    auto makeSpin = [this, paramWidget](double min, double max, double val, const QString &suffix) {
        auto *spin = new QDoubleSpinBox(paramWidget);
        spin->setRange(min, max);
        spin->setDecimals(1);
        spin->setValue(val);
        spin->setSuffix(suffix);
        spin->setFixedWidth(120);
        spin->setStyleSheet("QDoubleSpinBox { background: #22262e; color: #e0e6ed;"
                            " border: 1px solid #3a4050; border-radius: 3px; padding: 2px 4px; font-size: 11px; }");
        connect(spin, &QDoubleSpinBox::valueChanged, this, &QuotePage::onParamsChanged);
        return spin;
    };

    // 材料单价组
    auto *priceGroup = new QGroupBox(QStringLiteral("材料单价"), paramWidget);
    priceGroup->setStyleSheet("QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
                              " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
                              "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    auto *priceForm = new QFormLayout(priceGroup);
    priceForm->setContentsMargins(8, 4, 8, 8);
    priceForm->setSpacing(6);
    priceForm->setLabelAlignment(Qt::AlignRight);
    priceForm->setFormAlignment(Qt::AlignTop);

    m_steelPrice = makeSpin(0.0, 999.0, m_quoteParams.steelPrice, QStringLiteral(" 元/kg"));
    m_cuPrice = makeSpin(0.0, 999.0, m_quoteParams.cuPrice, QStringLiteral(" 元/kg"));
    m_oilPrice = makeSpin(0.0, 999.0, m_quoteParams.oilPrice, QStringLiteral(" 元/kg"));
    m_tankPrice = makeSpin(0.0, 999.0, m_quoteParams.tankPrice, QStringLiteral(" 元/kg"));
    priceForm->addRow(QStringLiteral("硅钢片"), m_steelPrice);
    priceForm->addRow(QStringLiteral("铜导线"), m_cuPrice);
    priceForm->addRow(QStringLiteral("绝缘油"), m_oilPrice);
    priceForm->addRow(QStringLiteral("油箱钢材"), m_tankPrice);
    paramLayout->addWidget(priceGroup);

    // 费用系数组
    auto *feeGroup = new QGroupBox(QStringLiteral("费用系数"), paramWidget);
    feeGroup->setStyleSheet(priceGroup->styleSheet());
    auto *feeForm = new QFormLayout(feeGroup);
    feeForm->setContentsMargins(8, 4, 8, 8);
    feeForm->setSpacing(6);
    feeForm->setLabelAlignment(Qt::AlignRight);

    m_purchasedSpin = makeSpin(0.0, 100.0, m_quoteParams.purchasedParts_pct, QStringLiteral(" %"));
    m_laborSpin = makeSpin(0.0, 100.0, m_quoteParams.labor_pct, QStringLiteral(" %"));
    m_mgmtSpin = makeSpin(0.0, 100.0, m_quoteParams.management_pct, QStringLiteral(" %"));
    m_profitSpin = makeSpin(0.0, 100.0, m_quoteParams.profit_pct, QStringLiteral(" %"));
    m_taxSpin = makeSpin(0.0, 100.0, m_quoteParams.tax_pct, QStringLiteral(" %"));
    m_miscSpin = makeSpin(0.0, 1e6, m_quoteParams.miscCost, QStringLiteral(" 元"));
    feeForm->addRow(QStringLiteral("外购件"), m_purchasedSpin);
    feeForm->addRow(QStringLiteral("人工"), m_laborSpin);
    feeForm->addRow(QStringLiteral("管理费"), m_mgmtSpin);
    feeForm->addRow(QStringLiteral("利润率"), m_profitSpin);
    feeForm->addRow(QStringLiteral("税率"), m_taxSpin);
    feeForm->addRow(QStringLiteral("其他费用"), m_miscSpin);
    paramLayout->addWidget(feeGroup);

    paramLayout->addStretch();
    splitter->addWidget(paramWidget);

    // ---- 右：报价明细表 + 汇总 ----
    auto *rightWidget = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_table = new QTableWidget(rightWidget);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("项目"), QStringLiteral("数量"), QStringLiteral("单位"),
        QStringLiteral("单价/比例"), QStringLiteral("金额(元)") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 5; ++c) {
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(
        "QTableWidget { background: #1a1d23; color: #c0c8d0; gridline-color: #3a4050;"
        " font-size: 12px; border: none; }"
        "QTableWidget::item { padding: 4px 8px; }"
        "QTableWidget::item:selected { background: rgba(0,188,212,0.25); }"
        "QHeaderView::section { background: #22262e; color: #8a9bb0; border: none;"
        " border-bottom: 1px solid #3a4050; padding: 6px 8px; font-size: 11px; }");
    rightLayout->addWidget(m_table, 1);

    // 汇总栏
    auto *summaryBar = new QWidget(rightWidget);
    summaryBar->setFixedHeight(56);
    summaryBar->setStyleSheet("background: #22262e; border-top: 1px solid #3a4050;");
    auto *sumLayout = new QHBoxLayout(summaryBar);
    sumLayout->setContentsMargins(12, 6, 12, 6);
    sumLayout->setSpacing(24);

    m_summaryLabel = new QLabel(summaryBar);
    m_summaryLabel->setStyleSheet("color: #e0e6ed; font-size: 12px;");
    m_summaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sumLayout->addWidget(m_summaryLabel, 1);
    rightLayout->addWidget(summaryBar);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 900});
    mainLayout->addWidget(splitter, 1);

    recalc();
}

void QuotePage::loadScheme(const TransformerParams &params, const CalcInput &input,
                           const CalcResult &result)
{
    m_params = params;
    m_input = input;
    m_result = result;
    m_hasScheme = result.valid;
    if (m_hasScheme) {
        m_schemeLabel->setText(
            QStringLiteral("当前方案：%1 kVA / 高压 %2 kV（计算单总重 %3 kg）")
                .arg(m_params.capacity_kVA)
                .arg(m_params.hvRatedVoltage_kV)
                .arg(QString::number(m_result.mass.totalWeight_kg, 'f', 0)));
        m_schemeLabel->setStyleSheet("color: #4dd0e1; font-size: 11px;");
    } else {
        m_schemeLabel->setText(QStringLiteral("方案无效，请重新计算"));
        m_schemeLabel->setStyleSheet("color: #e57373; font-size: 11px;");
    }
    recalc();
}

void QuotePage::onParamsChanged()
{
    // 从控件回填参数并重算
    m_quoteParams.steelPrice = m_steelPrice->value();
    m_quoteParams.cuPrice = m_cuPrice->value();
    m_quoteParams.oilPrice = m_oilPrice->value();
    m_quoteParams.tankPrice = m_tankPrice->value();
    m_quoteParams.purchasedParts_pct = m_purchasedSpin->value();
    m_quoteParams.labor_pct = m_laborSpin->value();
    m_quoteParams.management_pct = m_mgmtSpin->value();
    m_quoteParams.profit_pct = m_profitSpin->value();
    m_quoteParams.tax_pct = m_taxSpin->value();
    m_quoteParams.miscCost = m_miscSpin->value();
    recalc();
}

void QuotePage::onSaveParams()
{
    if (QuoteParams::saveToFile(QuoteCalculator::defaultParamsPath(), m_quoteParams)) {
        QMessageBox::information(this, QStringLiteral("保存参数"),
                                 QStringLiteral("报价参数已保存：\n%1")
                                     .arg(QDir::toNativeSeparators(
                                          QuoteCalculator::defaultParamsPath())));
    } else {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入参数文件，请检查目录权限"));
    }
}

void QuotePage::onExportQuote()
{
    if (!m_quote.valid) {
        QMessageBox::information(this, QStringLiteral("导出报价单"),
                                 QStringLiteral("暂无可导出的报价，请先载入方案"));
        return;
    }
    const QString defaultName = QStringLiteral("quote_%1.txt")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出报价单"), defaultName,
        QStringLiteral("文本文件 (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    // 使用 QFile 需 include；导出为 UTF-8 带换行的纯文本
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QString text;
        text += QStringLiteral("产品报价单\n");
        text += QStringLiteral("========================================\n");
        text += QStringLiteral("生成时间：%1\n")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm")));
        text += QStringLiteral("容量：%1 kVA　高压：%2 kV　低压：%3 kV\n")
                    .arg(m_params.capacity_kVA)
                    .arg(m_params.hvRatedVoltage_kV)
                    .arg(m_params.lvRatedVoltage_kV);
        text += QStringLiteral("========================================\n");
        for (const QuoteLine &line : m_quote.lines) {
            if (line.isFee) {
                text += QStringLiteral("%1　比例 %2%　金额 %3 元\n")
                            .arg(line.name)
                            .arg(QString::number(line.quantity, 'f', 1))
                            .arg(money(line.amount));
            } else {
                text += QStringLiteral("%1　数量 %2 kg　单价 %3 元/kg　金额 %4 元\n")
                            .arg(line.name)
                            .arg(qty(line.quantity))
                            .arg(money(line.unitPrice))
                            .arg(money(line.amount));
            }
        }
        text += QStringLiteral("========================================\n");
        text += QStringLiteral("材料成本小计：%1 元\n").arg(money(m_quote.materialCost));
        text += QStringLiteral("费用小计：%1 元\n").arg(money(m_quote.feeCost));
        text += QStringLiteral("成本合计：%1 元\n").arg(money(m_quote.costTotal));
        text += QStringLiteral("利润：%1 元\n").arg(money(m_quote.profit));
        text += QStringLiteral("税额：%1 元\n").arg(money(m_quote.tax));
        text += QStringLiteral("含税出厂报价：%1 元\n").arg(money(m_quote.quotePrice));
        f.write(text.toUtf8());
        f.close();
        QMessageBox::information(this, QStringLiteral("导出成功"),
                                 QStringLiteral("报价单已导出：\n%1")
                                     .arg(QDir::toNativeSeparators(path)));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法创建文件，请检查路径权限"));
    }
}

void QuotePage::recalc()
{
    m_table->clearContents();
    m_table->setRowCount(0);

    if (!m_hasScheme) {
        m_summaryLabel->setText(QStringLiteral(
            "未载入方案：请先在优化设计中完成计算并确认方案"));
        return;
    }

    m_quote = QuoteCalculator::calculate(m_params, m_result, m_quoteParams);
    if (!m_quote.valid) {
        m_summaryLabel->setText(QStringLiteral("报价计算失败"));
        return;
    }

    for (const QuoteLine &line : m_quote.lines) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *nameItem = new QTableWidgetItem(line.name);
        nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_table->setItem(row, 0, nameItem);
        auto *qtyItem = new QTableWidgetItem(
            line.isFee ? QStringLiteral("—") : qty(line.quantity));
        qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 1, qtyItem);
        auto *unitItem = new QTableWidgetItem(line.unit);
        unitItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 2, unitItem);
        auto *priceItem = new QTableWidgetItem(
            line.isFee ? QStringLiteral("—") : money(line.unitPrice));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 3, priceItem);
        auto *amtItem = new QTableWidgetItem(money(line.amount));
        amtItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 4, amtItem);
    }

    m_summaryLabel->setText(QStringLiteral(
        "材料成本 %1 元　费用 %2 元　成本合计 %3 元　利润 %4 元　税额 %5 元　含税报价 %6 元")
        .arg(money(m_quote.materialCost), money(m_quote.feeCost),
             money(m_quote.costTotal), money(m_quote.profit),
             money(m_quote.tax), money(m_quote.quotePrice)));
}
