#ifndef PARAMTABLEWIDGET_H
#define PARAMTABLEWIDGET_H
// 参数编辑表格（展示并编辑变压器设计输入参数，按结构配置动态切换行；
// 设计变量节（铁芯/绕组/主空道）与 CalcInput 双向同步）

#include <QTableWidget>
#include <QHash>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "CalcInput.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;

class ParamTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit ParamTableWidget(QWidget *parent = nullptr);
    // 根据结构配置动态加载参数（不同铁芯/绕组显示不同行），
    // 设计变量节初值取自 input（默认即 SB20-M-630-10）；
    // proMode=true 时追加工艺/油道/损耗/油箱四节高级参数（专业模式）
    void loadParamsForConfig(const TransformerParams &params, const StructureConfig &config,
                             const CalcInput &input, bool proMode = false, bool designConnectionUi = false);
    TransformerParams getParams() const;
    // 当前联结组别是否能同时用于电磁计算与现有标准损耗表。
    bool hasSupportedConnectionGroup() const;
    bool hasValidSteelGrade() const;
    // 从表格设计变量节读回 CalcInput（未绑定或非法输入的域保持原值）
    void saveToInput(CalcInput &input) const;
    // 计算边界统一校验并提交完整型号；失败时不改变调用方输入。
    bool collectForCalculation(TransformerParams &params, CalcInput &input, QString &error,
                               bool interactive = true);
    // 方案回显与输出元数据必须来自该方案，而不是寻优基准额定值。
    static TransformerParams paramsForInput(const TransformerParams &base, const CalcInput &input);
    QString standardStatus() const { return m_standardStatus; }
    void setCalculationConfig(const StructureConfig &config) {
        m_config = config;
        applyModelLinkage();
        updateHvCoilHints();
    }

signals:
    // 内置叠铁芯损耗标准表的适用状态；阻抗始终保留当前设置。
    void stdValuesUpdated(const QString &summary);

private:
    // 复合产品型号：型号-M-容量/高压额定电压-低压额定电压
    bool parseCompositeModel(const QString &text, bool commitLowVoltage);
    void applyModelLinkage();
    QString standardKey() const;
    QString standardUnavailableReason() const;
    QString selectedSteelGrade() const;
    void updateSteelThickness();
    void updateWireInsulation();
    void updateWireForm();
    void updateLvTurnsRecommendation();
    void updateYokePiece1();
    void updateTestVoltageHints();
    void updateHvCoilHints(); // 只更新固定行说明与悬停提示，不重建表格或改数值
    void updateDesignConnection(); // 仅设计输入页，联结组别与基础电量联动。
    bool m_designConnectionUi = false;
    QComboBox *m_designConnectionCombo = nullptr;
    int m_designConnectionRow = -1;
    int m_designPhaseRow = -1;
    int m_testVoltageRow = -1;
    QComboBox *m_yokePiece1ModeCombo = nullptr;
    double m_manualYokePiece1_mm = 4.0;
    CalcInput m_recommendationBaseInput;
    int m_recommendationRow = -1;
    QComboBox *m_wireInsulationCombo = nullptr;
    QComboBox *m_wireFormCombo = nullptr;
    QComboBox *m_roundWireSpecCombo = nullptr;
    bool m_roundWireActive = false;
    double m_unlistedRoundDiameter_mm = 0.0; // 保留导入原值，不从格式化文本还原
    QString m_flatWireWidth = QStringLiteral("2.05");
    QString m_flatWireThick = QStringLiteral("5.52");
    QString m_flatWireInsulation = QStringLiteral("QZB-2/130");
    double m_customWireInsulAdd_mm = 0.15;
    QLineEdit *m_productModelEdit = nullptr;
    QSpinBox *m_tapPlusSpin = nullptr;
    QSpinBox *m_tapMinusSpin = nullptr;
    QDoubleSpinBox *m_tapStepSpin = nullptr;
    QComboBox *m_steelGradeCombo = nullptr;
    int m_steelCurveRangeRow = -1;
    QComboBox *m_hvMaterialCombo = nullptr;
    QComboBox *m_lvMaterialCombo = nullptr;
    QString m_modelSeries;
    double m_modelCapacity_kVA = 630.0;
    double m_modelHvRated_kV = 10.0;
    double m_modelLvRated_kV = 0.4;
    QString m_lastLinkageKey;
    TransformerParams m_baseParams;
    StructureConfig m_config;
    bool m_lossStandardsManual = false;
    QString m_standardStatus;
    bool m_loading = false;               // 加载期间抑制联动信号

    void setupTable();
    // advanced=true 时节标题使用琥珀色调，与一~六节（青蓝调）区分高级参数
    void addSectionRow(int row, const QString &title,
                       const QString &optName = {}, const QString &optValue = {},
                       bool advanced = false);
    void addParamRow(int row, const QString &name, const QString &value,
                     const QString &optName = {}, const QString &optValue = {});
    // 添加设计变量行：左值绑定 key（数值列），右值绑定 optKey（选项列）
    void addInputRow(int row, const QString &name, const QString &value,
                     const QString &optName, const QString &optValue,
                     const QString &key, const QString &optKey);
    void bindInput(const QString &key, int row, int col);
    // 专业模式追加的高级参数节（七~十）
    void addProModeSections(int &row, const CalcInput &input);

    QHash<QString, QPair<int, int>> m_inputRefs;  // 参数键 → {行, 列}（含静态节与设计变量）
};

#endif // PARAMTABLEWIDGET_H
