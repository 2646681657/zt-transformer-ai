#include "SchemeTableWidget.h"
#include <QHeaderView>

SchemeTableWidget::SchemeTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupColumns();
}

void SchemeTableWidget::setupColumns()
{
    setColumnCount(19);
    QStringList headers = {
        "#", "方案序号", "主材成本\n铜铁油", "铜铁", "铁芯直径\n铁芯矩轴",
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
}

void SchemeTableWidget::addResult(const OptimizationResult &r)
{
    int row = rowCount();
    insertRow(row);
    int col = 0;
    setItem(row, col++, new QTableWidgetItem(QString::number(row + 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.schemeIdx)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.costCuFeOil, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.costCuFe, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.coreD, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.coreL, 'f', 2)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.lvTurns)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.lvRuleT, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.lvRuleW, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.hvRuleT, 'f', 2)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.hvRuleW, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.hvLayers)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.lvOilDucts)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.hvOilDucts)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.lvToYoke, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.mainDuct, 'f', 1)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.lvHalfOilDucts)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.hvHalfOilDucts)));
    setItem(row, col++, new QTableWidgetItem(QString::number(r.lvHalfDist, 'f', 1)));
}

void SchemeTableWidget::clearResults()
{
    setRowCount(0);
}
