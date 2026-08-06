#ifndef SCHEMETABLEWIDGET_H
#define SCHEMETABLEWIDGET_H
// 方案结果表格（展示优化器输出的多个方案对比数据）

#include <QTableWidget>
#include <QVector>
#include "OptimizationResult.h"

class SchemeTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit SchemeTableWidget(QWidget *parent = nullptr);
    // 追加一个优化方案到表格末尾
    void addResult(const OptimizationResult &result);
    void clearResults();

private:
    void setupColumns();
};

#endif // SCHEMETABLEWIDGET_H
