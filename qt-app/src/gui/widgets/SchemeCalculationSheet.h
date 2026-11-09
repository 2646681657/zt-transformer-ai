#ifndef SCHEMECALCULATIONSHEET_H
#define SCHEMECALCULATIONSHEET_H

#include <QWidget>
#include "CalcResult.h"
#include "CalcInput.h"
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>

class ParamTableWidget;
class QPrinter;
class QTableWidget;
class QLabel;
class QTableWidgetItem;
class QComboBox;
class QLineEdit;
class QPushButton;
class TankDimensionsPanel;
class QTextBrowser;
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
    bool exportExcel(const QString &path, const CalcInput &input, const TransformerParams &params,
                     const CalcResult &result, QString &error) const;
    void setSupplementParametersVisible(bool visible);
    void setOutputMode(); // 只读原单，隐藏辅助输入和说明
    void printTo(QPrinter *printer) const;
    ParamTableWidget *inputTable() const { return m_input; }
    void loadResult(const CalcResult &result);
    void refreshResult(const CalcResult &result); // 自动计算只刷新值，保留正在编辑的控件。
    void showCalculationError(const QString &error);
    bool inputsReady(QString &error) const;
    bool captureCoilForm(CalcInput &input, QString &error) const;
    void markPending();
    void refreshInputValues();
    QString partialPreviewStatus() const { return m_partialStatus; }

signals:
    void inputEdited();

private:
    bool eventFilter(QObject *source, QEvent *event) override;
    QWidget *mirrorEditor(QWidget *source);
    void rebuildSheet();
    void mergeFieldSpace();
    void fitTextRows();
    void fitColumnsToViewport();
    bool m_columnFitPending = false;
    void updateTankPanel(const QMap<QString, QPair<double, int>> *overrides = nullptr);
    QString tankSelectionError() const;
    QString coilSelectionError() const;
    void syncInputs();
    void showSupplement();
    void refreshSupplementDetails();
    bool refreshPartialPreview();
    bool editorsReady(QString &error) const;
    struct Binding { int row; int col; QTableWidgetItem *cell; };
    ParamTableWidget *m_input = nullptr;
    QTableWidget *m_results = nullptr;
    QLabel *m_resultStatus = nullptr;
    QPointer<QLineEdit> m_an3Raw;
    QPointer<QLabel> m_an3Status;
    bool m_an3Invalid = false;
    void syncAn3Controls();
    CalcResult m_snapshot;
    CalcInput m_previewBaseInput;
    QString m_partialStatus;
    bool m_partialActive = false;
    QString m_coreShapeName;
    QString m_coolingName;
    QPointer<QComboBox> m_coreShapeCombo;
    QPointer<QComboBox> m_yokeShapeCombo;
    QPointer<QComboBox> m_tankFormCombo;
    QPointer<QComboBox> m_coilFormCombo;
    QVector<Binding> m_bindings;
    QHash<QObject *, QPointer<QWidget>> m_editors;
    QMultiHash<QString, QTableWidgetItem *> m_resultBindings;
    QPointer<TankDimensionsPanel> m_tankPanel;
    QHash<QTableWidgetItem *, QPointer<QLineEdit>> m_panelInputs;
    QPushButton *m_supplementButton = nullptr;
    QSet<int> m_supplementRows;
    QStringList m_supplementKeys;
    QPointer<QTextBrowser> m_supplementDetails;
};

#endif
