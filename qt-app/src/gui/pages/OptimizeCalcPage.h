#ifndef OPTIMIZECALCPAGE_H
#define OPTIMIZECALCPAGE_H
// 参数设置页（Ribbon选型 + 参数表格编辑，确认后进入计算）

#include <QWidget>
#include <QVector>
#include "TransformerParams.h"
#include "StructureConfig.h"

class RibbonBar;
class RibbonGroup;
class ParamTableWidget;
class SidebarPanel;
class QTextEdit;
class QPushButton;

class OptimizeCalcPage : public QWidget {
    Q_OBJECT
public:
    explicit OptimizeCalcPage(QWidget *parent = nullptr);
    TransformerParams currentParams() const { return m_params; }
    StructureConfig currentConfig() const { return m_config; }
    // 设置变压器大类和绕组工艺，刷新页面内容
    void setStructureConfig(const StructureConfig &config);

signals:
    void navigateToEnterCalc();
    void navigateBack();

private slots:
    void onEnterCalcClicked();
    void onSelectionChanged();

private:
    void setupRibbon();
    void setupMainArea();
    void updateConfigFromRibbon();
    void refreshParamTable();

    RibbonBar *m_ribbon;
    ParamTableWidget *m_paramTable;
    SidebarPanel *m_sidebar;
    QTextEdit *m_helpPanel;
    QPushButton *m_navButton;
    TransformerParams m_params;
    StructureConfig m_config;
    QVector<RibbonGroup *> m_selectGroups;
};

#endif // OPTIMIZECALCPAGE_H
