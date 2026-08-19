#ifndef PARAMTABLEWIDGET_H
#define PARAMTABLEWIDGET_H
// 参数编辑表格（展示并编辑变压器设计输入参数，按结构配置动态切换行；
// 设计变量节（铁芯/绕组/主空道）与 CalcInput 双向同步）

#include <QTableWidget>
#include <QHash>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "CalcInput.h"

class ParamTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit ParamTableWidget(QWidget *parent = nullptr);
    // 根据结构配置动态加载参数（不同铁芯/绕组显示不同行），
    // 设计变量节初值取自 input（默认即 SB20-M-630-10）
    void loadParamsForConfig(const TransformerParams &params, const StructureConfig &config,
                             const CalcInput &input);
    TransformerParams getParams() const;
    // 从表格设计变量节读回 CalcInput（未绑定或非法输入的域保持原值）
    void saveToInput(CalcInput &input) const;

private:
    void setupTable();
    void addSectionRow(int row, const QString &title,
                       const QString &optName = {}, const QString &optValue = {});
    void addParamRow(int row, const QString &name, const QString &value,
                     const QString &optName = {}, const QString &optValue = {});
    // 添加设计变量行：左值绑定 key（数值列），右值绑定 optKey（选项列）
    void addInputRow(int row, const QString &name, const QString &value,
                     const QString &optName, const QString &optValue,
                     const QString &key, const QString &optKey);
    void bindInput(const QString &key, int row, int col);

    QHash<QString, QPair<int, int>> m_inputRefs;  // 设计变量键 → {行, 列}
};

#endif // PARAMTABLEWIDGET_H
