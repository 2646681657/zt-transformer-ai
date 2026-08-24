#ifndef LOSSCALCPAGE_H
#define LOSSCALCPAGE_H
// 损耗计算器（程序工具子页）：独立计算空载损耗和负载损耗
// 空载：铁芯重量 × 单位铁损 × 工艺系数
// 负载：电阻损耗(I²R) + 附加损耗，×(1+杂散系数)

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QComboBox;
class QTabWidget;

class LossCalcPage : public QWidget {
    Q_OBJECT
public:
    explicit LossCalcPage(QWidget *parent = nullptr);

private slots:
    void onCalcNoLoad();
    void onCalcLoadLoss();

private:
    void setupUi();
    QWidget *createNoLoadTab();
    QWidget *createLoadLossTab();

    // 空载损耗输入
    QDoubleSpinBox *m_coreWeight = nullptr;      // 硅钢片总重 kg
    QDoubleSpinBox *m_lossPerKg = nullptr;       // 单位铁损 W/kg
    QDoubleSpinBox *m_craftCoef = nullptr;       // 工艺系数

    // 负载损耗输入
    QDoubleSpinBox *m_hvResistance = nullptr;   // 高压电阻 Ω
    QDoubleSpinBox *m_lvResistance = nullptr;    // 低压电阻 Ω
    QDoubleSpinBox *m_hvCurrent = nullptr;       // 高压相电流 A
    QDoubleSpinBox *m_lvCurrent = nullptr;       // 低压相电流 A
    QDoubleSpinBox *m_hvExtraLoss = nullptr;     // 高压附加损耗 W
    QDoubleSpinBox *m_lvExtraLoss = nullptr;     // 低压附加损耗 W
    QDoubleSpinBox *m_strayFactor = nullptr;     // 杂散损耗系数 %

    // 结果
    QLabel *m_noLoadResult = nullptr;
    QLabel *m_loadLossResult = nullptr;
};

#endif // LOSSCALCPAGE_H
