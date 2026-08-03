#ifndef OPTIMIZECALCPAGE_H
#define OPTIMIZECALCPAGE_H

#include <QWidget>
#include <QVector>
#include "TransformerParams.h"

class RibbonBar;
class RibbonGroup;
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

private slots:
    void onEnterCalcClicked();

private:
    void setupRibbon();
    void setupMainArea();
    RibbonBar *m_ribbon;
    ParamTableWidget *m_paramTable;
    SidebarPanel *m_sidebar;
    QTextEdit *m_helpPanel;
    TransformerParams m_params;
    QVector<RibbonGroup *> m_selectGroups;
};

#endif // OPTIMIZECALCPAGE_H
