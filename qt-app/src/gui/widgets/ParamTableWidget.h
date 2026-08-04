#ifndef PARAMTABLEWIDGET_H
#define PARAMTABLEWIDGET_H
// 参数编辑表格（展示并编辑变压器设计输入参数，按结构配置动态切换行）

#include <QTableWidget>
#include "TransformerParams.h"
#include "StructureConfig.h"

class ParamTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit ParamTableWidget(QWidget *parent = nullptr);
    void loadParams(const TransformerParams &params);
    // 根据结构配置动态加载参数（不同铁芯/绕组显示不同行）
    void loadParamsForConfig(const TransformerParams &params, const StructureConfig &config);
    TransformerParams getParams() const;

private:
    void setupTable();
    void addSectionRow(int row, const QString &title,
                       const QString &optName = {}, const QString &optValue = {});
    void addParamRow(int row, const QString &name, const QString &value,
                     const QString &optName = {}, const QString &optValue = {});
};

#endif // PARAMTABLEWIDGET_H
