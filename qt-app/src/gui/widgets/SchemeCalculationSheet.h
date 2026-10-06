#ifndef SCHEMECALCULATIONSHEET_H
#define SCHEMECALCULATIONSHEET_H

#include <QWidget>
#include "CalcResult.h"
#include <QHash>
#include <QPointer>

class ParamTableWidget;
class QTableWidget;
class QLabel;
class QTableWidgetItem;
struct TransformerParams;
struct StructureConfig;
struct CalcInput;

// 仅用于方案选择弹窗：复用输入联动，不改变主输入页的表格布局。
class SchemeCalculationSheet : public QWidget {
    Q_OBJECT
public:
    SchemeCalculationSheet(const TransformerParams &params, const StructureConfig &config,
                           const CalcInput &input, const CalcResult &result, bool proMode,
                           QWidget *parent = nullptr);
    ParamTableWidget *inputTable() const { return m_input; }
    void loadResult(const CalcResult &result);
    void refreshResult(const CalcResult &result); // 自动计算只刷新值，保留正在编辑的控件。
    void showCalculationError(const QString &error);
    bool inputsReady(QString &error) const;
    void markPending();
    void refreshInputValues() { syncInputs(); }

signals:
    void inputEdited();

private:
    bool eventFilter(QObject *source, QEvent *event) override;
    QWidget *mirrorEditor(QWidget *source);
    void rebuildSheet();
    void syncInputs();
    struct Binding { int row; int col; QTableWidgetItem *cell; };
    ParamTableWidget *m_input = nullptr;
    QTableWidget *m_results = nullptr;
    QLabel *m_resultStatus = nullptr;
    CalcResult m_snapshot;
    QVector<Binding> m_bindings;
    QHash<QObject *, QPointer<QWidget>> m_editors;
    QHash<QString, QTableWidgetItem *> m_resultBindings;
};

#endif
