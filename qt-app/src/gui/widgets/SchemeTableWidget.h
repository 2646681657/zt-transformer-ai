#ifndef SCHEMETABLEWIDGET_H
#define SCHEMETABLEWIDGET_H
// 方案结果表格（展示优化器输出的多个方案对比数据；
// 行内「选择」按钮仅标记待确认方案（高亮互斥），确认动作由外部"方案确认"触发）

#include <QTableWidget>
#include <QVector>
#include "OptimizationResult.h"

class QPushButton;

class SchemeTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit SchemeTableWidget(QWidget *parent = nullptr);
    // 追加一个优化方案到表格末尾
    void addResult(const OptimizationResult &result);
    void clearResults();
    // 重算后刷新指定行数值列（保留行内「选择」按钮）
    void updateResult(int row, const OptimizationResult &result);
    // 点亮指定行的「选择」按钮（方案参数弹窗确认后调用）
    void markRow(int row);

    // 行内「选择」按钮当前标记的行（未标记返回 -1；
    // 按钮随排序移动，按控件指针反查实际行号）
    int markedRow() const;

signals:
    // 行内「选择」按钮点击：请求打开方案参数弹窗（确认后由 markRow 点亮按钮）
    void schemeSelected(int row);

private:
    void setupColumns();
    // 按列填充数值单元格（addResult/updateResult 共用）
    void fillRowItems(int row, const OptimizationResult &result);
    // 高亮指定按钮并取消其余行的标记
    void markButton(QPushButton *btn);

    QPushButton *m_markedBtn = nullptr;   // 当前标记的行内按钮（nullptr=无标记）
};

#endif // SCHEMETABLEWIDGET_H
