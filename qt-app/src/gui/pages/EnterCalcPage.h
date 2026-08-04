#ifndef ENTERCALCPAGE_H
#define ENTERCALCPAGE_H
// 计算主页（包含优化/方案/打印三个Tab，驱动优化器执行）

#include <QWidget>
#include <QTabWidget>
#include "TransformerParams.h"
#include "StructureConfig.h"

class RibbonBar;
class SidebarPanel;
class SchemeTableWidget;
class PrintTableWidget;
class IOptimizer;

class EnterCalcPage : public QWidget {
    Q_OBJECT
public:
    explicit EnterCalcPage(QWidget *parent = nullptr);
    void setParams(const TransformerParams &params) { m_params = params; }

signals:
    void navigateBack();

private slots:
    void onTabChanged(int index);
    // 启动优化器执行异步寻优计算
    void onStartOptimize();

private:
    void setupOptimizeTab();
    void setupSchemeTab();
    void setupPrintTab();
    void buildOptimizeRibbon();
    void buildSchemeRibbon();
    void buildPrintRibbon();

    QTabWidget *m_tabWidget;
    RibbonBar *m_optimizeRibbon;
    RibbonBar *m_schemeRibbon;
    RibbonBar *m_printRibbon;
    QWidget *m_ribbonStack;
    SchemeTableWidget *m_schemeTable;
    PrintTableWidget *m_printTable;
    TransformerParams m_params;
    StructureConfig m_config;
    IOptimizer *m_optimizer = nullptr;
};

#endif // ENTERCALCPAGE_H
