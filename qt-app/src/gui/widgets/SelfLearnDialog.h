#ifndef SELFLEARNDIALOG_H
#define SELFLEARNDIALOG_H
// AI自学习入口：保留原有实测对比，另设独立学习样本和训练/模型状态页。
// 样本模块不自动迁移历史0值，不接管原计算；真实训练与推理在后续接入。

#include <QDialog>
#include "SchemeStore.h"

class QTableWidget;
class QPushButton;
class QLabel;
class LearningWorkspace;
struct CalcResult;

class SelfLearnDialog : public QDialog {
    Q_OBJECT
public:
    explicit SelfLearnDialog(QWidget *parent = nullptr);
    void reject() override;

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
    LearningWorkspace *m_learning = nullptr;

    bool m_hasScheme = false;               // 是否已选方案
    SchemeStore::SchemeEntry m_entry;       // 当前选中方案（含实测数据）
};

#endif // SELFLEARNDIALOG_H
