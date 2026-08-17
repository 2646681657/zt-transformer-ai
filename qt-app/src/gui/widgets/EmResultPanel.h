#ifndef EMRESULTPANEL_H
#define EMRESULTPANEL_H
// 电磁计算结果面板（铁芯/绕组/阻抗/温升/重量成本五组页签展示 CalcResult）

#include <QTabWidget>
#include "CalcResult.h"

class QTableWidget;

class EmResultPanel : public QTabWidget {
    Q_OBJECT
public:
    explicit EmResultPanel(QWidget *parent = nullptr);

    // 填充一次完整电磁计算结果
    void loadResult(const CalcResult &result);

    // 结果文本化（保存计算单用）
    static QString resultText(const CalcResult &result);

private:
    QTableWidget *createPage(const QString &title);
    static void fillPage(QTableWidget *page, const QVector<QStringList> &rows);
    // 按组组织结果行：{ 组名, { {参数名, 数值, 单位}, ... } }
    static QVector<QPair<QString, QVector<QStringList>>> buildGroups(const CalcResult &result);

    QTableWidget *m_coreTab = nullptr;
    QTableWidget *m_windingTab = nullptr;
    QTableWidget *m_impedanceTab = nullptr;
    QTableWidget *m_thermalTab = nullptr;
    QTableWidget *m_massTab = nullptr;
};

#endif // EMRESULTPANEL_H
