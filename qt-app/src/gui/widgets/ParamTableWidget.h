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
                             const CalcInput &input, bool proMode = false);
    TransformerParams getParams() const;
    // 当前联结组别是否能同时用于电磁计算与现有标准损耗表。
    bool hasSupportedConnectionGroup() const;
    bool hasValidSteelGrade() const;
    // 从表格设计变量节读回 CalcInput（未绑定或非法输入的域保持原值）
    void saveToInput(CalcInput &input) const;

signals:
    // 型号/容量联动：按 GB 20052-2024 叠铁芯标准值自动覆盖损耗/阻抗标准值后发出
    void stdValuesUpdated(const QString &summary);

private:
    // 复合产品型号：型号-M-容量/高压额定电压-低压额定电压
    bool parseCompositeModel(const QString &text, bool commitLowVoltage);
    void applyModelLinkage();
    QString selectedSteelGrade() const;
    void updateSteelThickness();
    QLineEdit *m_productModelEdit = nullptr;
    QSpinBox *m_tapPlusSpin = nullptr;
    QSpinBox *m_tapMinusSpin = nullptr;
    QDoubleSpinBox *m_tapStepSpin = nullptr;
    QComboBox *m_steelGradeCombo = nullptr;
    QComboBox *m_hvMaterialCombo = nullptr;
    QComboBox *m_lvMaterialCombo = nullptr;
    QString m_modelSeries;
    double m_modelCapacity_kVA = 630.0;
    double m_modelHvRated_kV = 10.0;
    double m_modelLvRated_kV = 0.4;
    QString m_lastLinkageKey;
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
