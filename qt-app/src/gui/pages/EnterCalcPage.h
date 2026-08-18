#ifndef ENTERCALCPAGE_H
#define ENTERCALCPAGE_H
// 计算主页（优化/方案/打印三个Tab，驱动电磁计算引擎执行并展示结果）

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "CalcInput.h"
#include "CalcResult.h"
#include "ElectromagneticEngine.h"

class RibbonBar;
class SidebarPanel;
class SchemeTableWidget;
class PrintTableWidget;
class EmResultPanel;

class EnterCalcPage : public QWidget {
    Q_OBJECT
public:
    explicit EnterCalcPage(QWidget *parent = nullptr);
    void setParams(const TransformerParams &params) { m_params = params; }

signals:
    void navigateBack();

private slots:
    void onTabChanged(int index);
    // 执行电磁计算全链路并填充结果面板/打印表/方案表
    void onRunEmCalc();
    // 对拍自检：SB20 计算单缓存值逐项对照
    void onSelfTest();
    // 保存当前电磁计算结果为计算单文本文件
    void onSaveCalcSheet();
    // ---- 方案选择 Tab ----
    // 按主材成本升序/降序排序方案表
    void onSortSchemes(bool ascending);
    // 按主材成本上限筛选（取消输入则清除筛选）
    void onFilterSchemes();
    // 弹出列显隐菜单
    void onSchemeColumnMenu();
    // 校验选中方案后跳转输出打印 Tab
    void onConfirmScheme();
    // ---- 输出打印 Tab ----
    // 快速打印（默认打印机直接出纸）/ 打印（弹打印机对话框）/ 打印预览
    void onQuickPrint();
    void onPrint();
    void onPrintPreview();
    // 打印表导出为 CSV（Excel 可直接打开）
    void onExportCsv();

private:
    void setupOptimizeTab();
    void setupSchemeTab();
    void setupPrintTab();
    void buildOptimizeRibbon();
    void buildSchemeRibbon();
    void buildPrintRibbon();
    // 当前计算结果映射为方案行，追加进方案表
    void appendScheme(const CalcInput &input, const CalcResult &result);

    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    RibbonBar *m_optimizeRibbon;
    RibbonBar *m_schemeRibbon;
    RibbonBar *m_printRibbon;
    QWidget *m_ribbonStack;
    QPushButton *m_navButton;
    SchemeTableWidget *m_schemeTable;
    PrintTableWidget *m_printTable;
    EmResultPanel *m_emResultPanel = nullptr;
    QLabel *m_statusBar = nullptr;
    TransformerParams m_params;
    StructureConfig m_config;
    ElectromagneticEngine m_engine;
    CalcResult m_emResult;
    bool m_hasResult = false;
};

#endif // ENTERCALCPAGE_H
