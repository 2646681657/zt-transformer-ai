#include "SchemeTableWidget.h"
#include <QHeaderView>
#include <QPushButton>

namespace {

// 行内「选择」按钮样式：常态（半透明）与标记态（高亮变亮）
void applyButtonStyle(QPushButton *btn, bool marked)
{
    if (marked) {
        btn->setStyleSheet(
            "QPushButton { background: #00bcd4; color: #0d1117;"
            "border: 1px solid #00bcd4; border-radius: 3px; font-size: 11px;"
            "padding: 2px 10px; font-weight: bold; }"
            "QPushButton:hover { background: #00a5bf; }");
    } else {
        btn->setStyleSheet(
            "QPushButton { background: rgba(0,188,212,0.15); color: #4dd0e1;"
            "border: 1px solid #3a4050; border-radius: 3px; font-size: 11px; padding: 2px 10px; }"
            "QPushButton:hover { background: #00bcd4; color: #0d1117; }");
    }
}

} // namespace

SchemeTableWidget::SchemeTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupColumns();
}

void SchemeTableWidget::setupColumns()
{
    setColumnCount(19);
    QStringList headers = {
        "选择", "方案序号", "主材成本\n铜铁油", "铜铁", "铁芯直径\n铁芯矩轴",
        "铁芯长轴\n与短轴比", "低压匝数", "低压线规厚", "低压线规宽",
        "高压线规厚", "高压线规宽", "高压线圈层数", "低压油道个数",
        "高压油道个数", "低压到铁扼最", "主空道尺寸", "低压半油道个",
        "高压半油道个", "低压半距"
    };
    setHorizontalHeaderLabels(headers);
    verticalHeader()->setVisible(false);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    horizontalHeader()->setStretchLastSection(true);
    setColumnWidth(0, 56);
}

void SchemeTableWidget::addResult(const OptimizationResult &r)
{
    int row = rowCount();
    insertRow(row);

    // 第 0 列：行内「选择」按钮。点击仅标记该方案（按钮变亮，其余行取消标记），
    // 确认动作由 Ribbon「方案确认」按钮触发；排序后按钮随行移动，
    // 回调时按 cellWidget 反查实际行号
    auto *btn = new QPushButton(QStringLiteral("选择"), this);
    btn->setCursor(Qt::PointingHandCursor);
    applyButtonStyle(btn, false);
    connect(btn, &QPushButton::clicked, this, [this, btn]() {
        markButton(btn);
        for (int i = 0; i < rowCount(); ++i) {
            if (cellWidget(i, 0) == btn) {
                emit schemeSelected(i);
                return;
            }
        }
    });
    setCellWidget(row, 0, btn);

    // 数值列用数值类型存储（非文本），保证升序/降序/取消排序
    // 都按数值比较而非字符串字典序（否则 "10" < "2"）
    const auto numItem = [](double v) {
        auto *it = new QTableWidgetItem;
        it->setData(Qt::DisplayRole, v);
        return it;
    };
    const auto intItem = [](int v) {
        auto *it = new QTableWidgetItem;
        it->setData(Qt::DisplayRole, v);
        return it;
    };

    int col = 1;
    setItem(row, col++, intItem(r.schemeIdx));
    setItem(row, col++, numItem(r.costCuFeOil));
    setItem(row, col++, numItem(r.costCuFe));
    setItem(row, col++, numItem(r.coreD));
    setItem(row, col++, numItem(r.coreL));
    setItem(row, col++, intItem(r.lvTurns));
    setItem(row, col++, numItem(r.lvRuleT));
    setItem(row, col++, numItem(r.lvRuleW));
    setItem(row, col++, numItem(r.hvRuleT));
    setItem(row, col++, numItem(r.hvRuleW));
    setItem(row, col++, intItem(r.hvLayers));
    setItem(row, col++, intItem(r.lvOilDucts));
    setItem(row, col++, intItem(r.hvOilDucts));
    setItem(row, col++, numItem(r.lvToYoke));
    setItem(row, col++, numItem(r.mainDuct));
    setItem(row, col++, intItem(r.lvHalfOilDucts));
    setItem(row, col++, intItem(r.hvHalfOilDucts));
    setItem(row, col++, numItem(r.lvHalfDist));
}

void SchemeTableWidget::markButton(QPushButton *btn)
{
    // 高亮目标按钮，取消其余行的标记（互斥：同一时刻仅一个待确认方案）
    for (int i = 0; i < rowCount(); ++i) {
        if (auto *b = qobject_cast<QPushButton *>(cellWidget(i, 0))) {
            applyButtonStyle(b, b == btn);
        }
    }
    m_markedBtn = btn;
}

int SchemeTableWidget::markedRow() const
{
    if (!m_markedBtn) {
        return -1;
    }
    for (int i = 0; i < rowCount(); ++i) {
        if (cellWidget(i, 0) == m_markedBtn) {
            return i;
        }
    }
    return -1;
}

void SchemeTableWidget::clearResults()
{
    m_markedBtn = nullptr;   // 行移除时 cellWidget 一并销毁，先置空防悬空
    setRowCount(0);
}
