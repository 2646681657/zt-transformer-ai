#ifndef PRINTTABLEWIDGET_H
#define PRINTTABLEWIDGET_H
// 输出打印表格（以双栏格式展示最终计算结果）

#include <QTableWidget>
#include "PrintOutputData.h"

class PrintTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit PrintTableWidget(QWidget *parent = nullptr);
    void loadData(const PrintOutputData &data);
};

#endif // PRINTTABLEWIDGET_H
