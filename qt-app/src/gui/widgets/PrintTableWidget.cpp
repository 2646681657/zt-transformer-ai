#include "PrintTableWidget.h"
#include <QHeaderView>
#include <QFont>

PrintTableWidget::PrintTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setColumnCount(7);
    QStringList headers = {"#", "参数名称", "数值", "单位", "参数名称", "数值", "单位"};
    setHorizontalHeaderLabels(headers);
    verticalHeader()->setVisible(false);
    setAlternatingRowColors(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    horizontalHeader()->setStretchLastSection(true);
    setColumnWidth(0, 30);
    setColumnWidth(1, 160);
    setColumnWidth(2, 100);
    setColumnWidth(3, 50);
    setColumnWidth(4, 160);
    setColumnWidth(5, 100);
}

void PrintTableWidget::loadData(const PrintOutputData &data)
{
    setRowCount(0);
    for (int i = 0; i < data.rows.size(); ++i) {
        const auto &r = data.rows[i];
        insertRow(i);
        setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));

        if (r.isSectionHeader) {
            auto *item = new QTableWidgetItem(r.leftName);
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setBackground(QColor("#e8f0ff"));
            setItem(i, 1, item);
            setSpan(i, 1, 1, 6);
        } else {
            setItem(i, 1, new QTableWidgetItem(r.leftName));
            setItem(i, 2, new QTableWidgetItem(r.leftValue));
            setItem(i, 3, new QTableWidgetItem(r.leftUnit));
            setItem(i, 4, new QTableWidgetItem(r.rightName));
            setItem(i, 5, new QTableWidgetItem(r.rightValue));
            setItem(i, 6, new QTableWidgetItem(r.rightUnit));
        }
    }
}
