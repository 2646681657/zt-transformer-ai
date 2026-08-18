#ifndef ENTERCALCPAGE_H
#define ENTERCALCPAGE_H
// 计算主页（优化/方案/打印三个Tab，驱动电磁计算引擎执行并展示结果）

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
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
    // 取消排序（恢复方案序号顺序）
    void onSortCancel();
    // 按主材成本上限筛选（取消输入则清除筛选）
    void onFilterSchemes();
    // 高级筛选：多条件组合（成本上限 + 性能约束）
    void onFilterAdvanced();
    // 选择筛选：按指定列值精确筛选
    void onFilterPick();
    // 清除全部筛选
    void onFilterCancel();
    // 弹出列显隐菜单（显示主要参数/设置合并模式共用）
    void onSchemeColumnMenu();
    // 切换排序列（主材成本/铜铁/铁芯直径/低压匝数）
    void onSwitchSortColumn();
    // 查找替换：定位并高亮指定方案序号
    void onFindScheme();
    // 设置合并模式：选择合并显示的主要参数列
    void onMergeSet();
    // 切换合并模式：全参数/仅主要参数
    void onMergeToggle();
    // 方案库比较：当前方案与已确认方案对比
    void onCompareLibrary();
    // 显示数量/显示方式变化
    void onShowCountChanged(int count);
    void onShowModeChanged(int index);
    // 观察完整参数勾选变化
    void onObserveToggled(bool checked);
    // 方案库/方案序号选择变化
    void onLibIndexChanged(int value);
    void onSchemeIndexChanged(int value);
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
    // 方案选择 Ribbon 控件（与图片布局一一对应）
    QCheckBox *m_showMainChk = nullptr;
    QCheckBox *m_observeChk = nullptr;
    QSpinBox *m_showCountSpin = nullptr;
    QComboBox *m_showModeCombo = nullptr;
    QSpinBox *m_libIndexSpin = nullptr;
    QSpinBox *m_schemeIndexSpin = nullptr;
    int m_sortCol = 2;          // 当前排序列（2=主材成本）
    bool m_mergeOn = false;     // 合并模式开关
    int m_confirmedRow = -1;    // 已确认方案行（方案库比较基准）
    TransformerParams m_params;
    StructureConfig m_config;
    ElectromagneticEngine m_engine;
    CalcResult m_emResult;
    bool m_hasResult = false;
};

#endif // ENTERCALCPAGE_H
