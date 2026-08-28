#ifndef OPTIMIZECALCPAGE_H
#define OPTIMIZECALCPAGE_H
// 参数设置页（Ribbon选型 + 参数表格编辑，确认后进入计算）

#include <QWidget>
#include <QVector>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "CalcInput.h"

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
    // 参数表中编辑的设计变量（进入计算时传给 EnterCalcPage）
    CalcInput currentInput() const { return m_input; }
    // 设置变压器大类和绕组工艺，刷新页面内容
    void setStructureConfig(const StructureConfig &config);

signals:
    void navigateToEnterCalc();
    void navigateBack();

private slots:
    void onEnterCalcClicked();
    void onSelectionChanged();
    void onImportParamsClicked();   // 导入基础技术参数表（Excel/CSV）
    void onVerifySheetClicked();   // 校验算单：约束预检

private:
    void setupRibbon();
    void setupMainArea();
    void updateConfigFromRibbon();
    void refreshParamTable();
    // 帮助面板文案随计算模式切换（正常/专业）
    void updateHelpPanel();
    void saveModePreference() const;
    // 应用方案设计变量到参数表（不动 Ribbon 结构选型）
    void applySchemeInput(const CalcInput &input);
    // 侧边栏方案按钮（0=推荐 1=保存我的 2=方案库 3=记忆库 4=上次方案）
    void onSchemeButtonClicked(int index);

    RibbonBar *m_ribbon;
    ParamTableWidget *m_paramTable;
    SidebarPanel *m_sidebar;
    QTextEdit *m_helpPanel;
    QPushButton *m_navButton;
    TransformerParams m_params;
    StructureConfig m_config;
    CalcInput m_input;              // 设计变量（默认即 SB20-M-630-10）
    QVector<RibbonGroup *> m_selectGroups;
    RibbonGroup *m_modeGroup = nullptr;   // 计算模式组（正常/专业）
};

#endif // OPTIMIZECALCPAGE_H
