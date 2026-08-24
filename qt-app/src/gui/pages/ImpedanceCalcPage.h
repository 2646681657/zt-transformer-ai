#ifndef IMPEDANCECALCPAGE_H
#define IMPEDANCECALCPAGE_H
// 阻抗计算器（程序工具子页）：输入绕组/漏磁参数，单独计算阻抗电压
// 公式源自 ElectromagneticEngine::calcImpedance，提取核心计算逻辑

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QComboBox;

class ImpedanceCalcPage : public QWidget {
    Q_OBJECT
public:
    explicit ImpedanceCalcPage(QWidget *parent = nullptr);

private slots:
    void onCalc();

private:
    void setupUi();

    // 输入参数
    QDoubleSpinBox *m_capacity = nullptr;       // 容量 kVA
    QDoubleSpinBox *m_lvTurns = nullptr;        // 低压匝数
    QDoubleSpinBox *m_lvCurrent = nullptr;      // 低压相电流 A
    QDoubleSpinBox *m_turnVoltage = nullptr;    // 匝电压 V
    QDoubleSpinBox *m_lambda = nullptr;          // 漏磁通道总厚 λ mm
    QDoubleSpinBox *m_hx = nullptr;             // 电抗高 mm
    QDoubleSpinBox *m_leakArea = nullptr;       // 漏磁面积 mm²
    QDoubleSpinBox *m_loadLoss = nullptr;       // 负载损耗 W（用于电阻压降）

    // 结果
    QLabel *m_resultLabel = nullptr;            // 阻抗电压结果
    QLabel *m_detailLabel = nullptr;            // 详细分解
};

#endif // IMPEDANCECALCPAGE_H
