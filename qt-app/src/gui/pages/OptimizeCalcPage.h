#ifndef OPTIMIZECALCPAGE_H
#define OPTIMIZECALCPAGE_H

#include <QWidget>
#include "TransformerParams.h"

class RibbonBar;
class ParamTableWidget;
class SidebarPanel;
class QTextEdit;

class OptimizeCalcPage : public QWidget {
    Q_OBJECT
public:
    explicit OptimizeCalcPage(QWidget *parent = nullptr);
    TransformerParams currentParams() const { return m_params; }

signals:
    void navigateToEnterCalc();
    void navigateBack();

private:
    void setupRibbon();
    void setupMainArea();
    RibbonBar *m_ribbon;
    ParamTableWidget *m_paramTable;
    SidebarPanel *m_sidebar;
    QTextEdit *m_helpPanel;
    TransformerParams m_params;
};

#endif // OPTIMIZECALCPAGE_H
