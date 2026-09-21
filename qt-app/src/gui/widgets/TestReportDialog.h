#ifndef TESTREPORTDIALOG_H
#define TESTREPORTDIALOG_H
// 试验实测数据录入/编辑对话框（AI 自学习第一步的数据源）。
// 录入试验报告的实测值：空载/负载损耗、阻抗电压、三项温升 + 试验日期

#include <QDialog>
#include "SchemeStore.h"

class QDoubleSpinBox;
class QDateEdit;

class TestReportDialog : public QDialog {
    Q_OBJECT
public:
    // 编辑已有数据（report.hasData=true 时预填）
    explicit TestReportDialog(const SchemeStore::TestReport &report,
                              QWidget *parent = nullptr);

    // 确认后的实测数据（hasData=true；取消返回传入的原值）
    SchemeStore::TestReport report() const { return m_report; }

private slots:
    void onSave();

private:
    void setupUi();

    SchemeStore::TestReport m_report;
    QDoubleSpinBox *m_noLoadLoss = nullptr;
    QDoubleSpinBox *m_loadLoss = nullptr;
    QDoubleSpinBox *m_impedance = nullptr;
    QDoubleSpinBox *m_oilTopRise = nullptr;
    QDoubleSpinBox *m_hvRise = nullptr;
    QDoubleSpinBox *m_lvRise = nullptr;
    QDateEdit *m_testDate = nullptr;
};

#endif // TESTREPORTDIALOG_H
