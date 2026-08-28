#ifndef SELFLEARNDIALOG_H
#define SELFLEARNDIALOG_H
// AI 自学习（第一步）：设计值 vs 试验实测值对比对话框。
// 从方案库/记忆库选择方案 → 引擎重算设计值 → 与录入的实测值逐项对比，
// 偏差百分比 + 超差高亮；支持在此录入/修改实测数据。
// 修正系数自动调整（第二步）等数据积累后接入

#include <QDialog>
#include "SchemeStore.h"

class QTableWidget;
class QPushButton;
class QLabel;
struct CalcResult;

class SelfLearnDialog : public QDialog {
    Q_OBJECT
public:
    explicit SelfLearnDialog(QWidget *parent = nullptr);

private slots:
    void onPickScheme();        // 选择方案（方案库/记忆库合并列表）
    void onEditReport();        // 录入/修改实测数据
    void refreshCompare();      // 重算设计值并刷新对比表

private:
    void setupUi();
    void fillCompareTable(const CalcResult &result);

    QTableWidget *m_table = nullptr;
    QPushButton *m_pickBtn = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_calcBtn = nullptr;
    QLabel *m_infoLabel = nullptr;

    bool m_hasScheme = false;               // 是否已选方案
    SchemeStore::SchemeEntry m_entry;       // 当前选中方案（含实测数据）
};

#endif // SELFLEARNDIALOG_H
