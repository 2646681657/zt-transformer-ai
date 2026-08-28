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
#include <QHash>
#include <optional>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "CalcInput.h"
#include "CalcResult.h"
#include "ElectromagneticEngine.h"
#include "IOptimizer.h"

class RibbonBar;
class RibbonButton;
class SidebarPanel;
class SchemeTableWidget;
class PrintTableWidget;
class EmResultPanel;
class GridOptimizer;

class EnterCalcPage : public QWidget {
    Q_OBJECT
public:
    explicit EnterCalcPage(QWidget *parent = nullptr);
    void setParams(const TransformerParams &params) { m_params = params; }
    // 设置设计变量（参数设置页编辑值，未设置时用 SB20-M-630-10 默认值）；
    // 同时刷新输出打印 Tab 初始计算单（构造时不预计算，避免显示默认方案结果）
    void setCalcInput(const CalcInput &input);
    // 设置结构配置（参数设置页 Ribbon 选型，寻优与查看弹窗使用）
    void setConfig(const StructureConfig &config) { m_config = config; }

signals:
    void navigateBack();
    // 程序选择导航按钮：跳转主界面（区别于 navigateBack 返回参数设置页）
    void dashboardRequested();
    // 方案确认后发出（携带确认方案完整数据，供主界面报价页联动）
    void schemeConfirmed(const TransformerParams &params, const CalcInput &input,
                         const CalcResult &result);

private slots:
    void onTabChanged(int index);
    // 执行电磁计算全链路并填充结果面板/打印表/方案表
    void onRunEmCalc();
    // 侧边栏方案按钮（0=推荐 1=保存我的 2=方案库 3=上一次）
    void onSchemeButtonClicked(int index);
    // ---- 异步寻优（开始运行/暂停/停止 按钮驱动 GridOptimizer）----
    void onOptimizeStart();
    void onOptimizePause();
    void onOptimizeStop();
    void onOptimizeProgress(int percent);
    // 候选方案入库（方案表）
    void onOptimizeCandidate(const OptimizeCandidate &candidate);
    // 寻优结束：最优方案加载到结果面板
    void onOptimizeFinished(bool stopped, const OptimizeCandidate &best,
                            int total, int valid);
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
    // 行内「选择」按钮点击：仅标记该方案（按钮变亮），不跳转
    void onSchemeSelected(int row);
    // ---- 方案库存储 ----
    // 批量保存当前方案表全部方案（设计变量 JSON 文件）
    void onSaveSchemes();
    // 打开方案库文件：重算结果后恢复方案表（可继续选择/确认/比较）
    void onLoadSchemes();
    // ---- 输出打印 Tab ----
    // 打印设置：选择默认打印机与页面方向
    void onPrintSetup();
    // 快速打印（默认打印机直接出纸）/ 打印（弹打印机对话框）/ 打印预览
    void onQuickPrint();
    void onPrint();
    void onPrintPreview();
    // 打开报价单：材料成本明细
    void onOpenQuote();
    // 打开计算单：完整计算结果文本
    void onOpenCalcSheet();
    // 打开三张参数表（绝缘半径/铁芯尺寸/性能比对）
    void onOpenInsulRadiusTable();
    void onOpenCoreSizeTable();
    void onOpenPerfCompareTable();
    // 打印表导出为 CSV（Excel 可直接打开）
    void onExportCsv();
    // 叠铁铁芯片下料表：铁芯叠积各级片宽/叠厚导出 CSV
    void onExportStackTable();
    // 计算单配置关联：展示输入与结果关键配置
    void onCalcSheetConfig();
    // 保存为软件计算单 / 自定义计算单
    void onSaveSoftwareSheet();
    void onSaveCustomSheet();
    // 一键导出《计算单》《成本清单》两份文件（按图号+型号归档）
    void onExportDocuments();

private:
    void setupOptimizeTab();
    void setupSchemeTab();
    void setupPrintTab();
    // 竖排"程序选择"导航按钮（点击返回主界面），三个 Tab 各自持有
    QPushButton *createNavButton(QWidget *parent);
    void buildOptimizeRibbon();
    void buildSchemeRibbon();
    void buildPrintRibbon();
    // 初始化设置组按钮的只读查看弹窗（各配置项当前值，修改入口在参数设置页）
    void showInitInfoDialog(int index);
    // 循环参数编辑对话框（寻优网格范围/步长，QSettings 持久化）；
    // 返回确认后的设置（取消返回空 optional）
    std::optional<OptimizationSettings> showLoopParamsDialog();
    // 读取持久化的寻优设置（未配置过返回默认值）
    OptimizationSettings loadOptimizeSettings() const;
    // 持久化寻优设置
    void saveOptimizeSettings(const OptimizationSettings &settings) const;
    // 当前计算结果映射为方案行，追加进方案表
    void appendScheme(const CalcInput &input, const CalcResult &result);
    // 确认指定行方案：记录基准并跳转输出打印 Tab
    void confirmSchemeAt(int row);

    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    RibbonBar *m_optimizeRibbon;
    RibbonBar *m_schemeRibbon;
    RibbonBar *m_printRibbon;
    QWidget *m_ribbonStack;
    RibbonButton *m_pauseBtn = nullptr;   // 暂停/继续 切换按钮
    GridOptimizer *m_optimizer = nullptr; // 网格寻优器（后台线程）
    OptimizationSettings m_optSettings;   // 寻优网格设置（循环参数对话框可配）
    bool m_optRunning = false;            // 寻优运行中标志
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
    int m_confirmedSchemeIdx = -1;   // 已确认方案序号（方案库比较基准，排序无关）
    // 方案序号 → 完整候选（input+result+方案行）：确认方案时取回该方案全部数据
    QHash<int, OptimizeCandidate> m_schemeData;
    TransformerParams m_params;
    StructureConfig m_config;
    ElectromagneticEngine m_engine;
    CalcResult m_emResult;
    CalcInput m_calcInput;       // 当前设计变量（参数设置页传入，默认 SB20）
    CalcInput m_lastInput;       // 最近一次计算实际使用的输入
    bool m_hasResult = false;
    QString m_defaultPrinterName;   // 打印设置选定的打印机
    bool m_landscape = false;       // 打印设置选定的横向
};

#endif // ENTERCALCPAGE_H
