#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H
// 系统设置页（程序工具子页）：计算精度 + 默认计算模式 + 报价参数 + 默认打印机 + AI 配置

#include <QWidget>
#include "QuoteCalculator.h"

class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);

private slots:
    void onSave();
    void onResetQuote();
    void onRestoreDefaults();
    void onTestLlm();

private:
    void setupUi();
    void loadSettings();

    // 计算精度
    QSpinBox *m_precisionSpin = nullptr;
    // 默认计算模式（与参数设置页 Ribbon 共用 optimize/proMode 偏好）
    QComboBox *m_calcModeCombo = nullptr;
    // 默认打印机
    QComboBox *m_printerCombo = nullptr;
    // 报价参数 - 材料单价
    QDoubleSpinBox *m_steelPrice = nullptr;
    QDoubleSpinBox *m_cuPrice = nullptr;
    QDoubleSpinBox *m_oilPrice = nullptr;
    QDoubleSpinBox *m_tankPrice = nullptr;
    // 报价参数 - 费用系数
    QDoubleSpinBox *m_purchasedPartsPct = nullptr;
    QDoubleSpinBox *m_laborPct = nullptr;
    QDoubleSpinBox *m_managementPct = nullptr;
    QDoubleSpinBox *m_profitPct = nullptr;
    QDoubleSpinBox *m_taxPct = nullptr;
    QDoubleSpinBox *m_miscCost = nullptr;
    // AI 配置
    QCheckBox *m_llmEnabled = nullptr;
    QLineEdit *m_llmKeyEdit = nullptr;
    QLineEdit *m_llmModelEdit = nullptr;
    QLineEdit *m_llmUrlEdit = nullptr;
    QPushButton *m_llmTestBtn = nullptr;

    QLabel *m_statusLabel = nullptr;
};

#endif // SETTINGSPAGE_H
