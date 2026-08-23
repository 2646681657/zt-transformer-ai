#ifndef QUOTEPAGE_H
#define QUOTEPAGE_H
// 产品报价页（内嵌主界面内容区）：基于已确认方案的材料用量生成报价单，
// 支持单价/费用系数调整并持久化，可导出报价单文本

#include <QWidget>
#include "CalcResult.h"
#include "CalcInput.h"
#include "TransformerParams.h"
#include "QuoteCalculator.h"

class QTableWidget;
class QLineEdit;
class QDoubleSpinBox;
class QLabel;

class QuotePage : public QWidget {
    Q_OBJECT
public:
    explicit QuotePage(QWidget *parent = nullptr);

    // 载入已确认方案（params+input+result）并刷新报价；
    // 无方案时显示提示
    void loadScheme(const TransformerParams &params, const CalcInput &input,
                    const CalcResult &result);
    // 上次载入的结果是否有效（用于主界面入口判断）
    bool hasScheme() const { return m_hasScheme; }

private slots:
    void onParamsChanged();
    void onSaveParams();
    void onExportQuote();

private:
    void setupUi();
    void recalc();

    // 报价参数编辑（单价 + 费用系数）
    QDoubleSpinBox *m_steelPrice = nullptr;
    QDoubleSpinBox *m_cuPrice = nullptr;
    QDoubleSpinBox *m_oilPrice = nullptr;
    QDoubleSpinBox *m_tankPrice = nullptr;
    QDoubleSpinBox *m_purchasedSpin = nullptr;
    QDoubleSpinBox *m_laborSpin = nullptr;
    QDoubleSpinBox *m_mgmtSpin = nullptr;
    QDoubleSpinBox *m_profitSpin = nullptr;
    QDoubleSpinBox *m_taxSpin = nullptr;
    QDoubleSpinBox *m_miscSpin = nullptr;

    QTableWidget *m_table = nullptr;      // 报价明细表
    QLabel *m_summaryLabel = nullptr;     // 汇总（成本/利润/税/报价）
    QLabel *m_schemeLabel = nullptr;      // 当前方案信息

    QuoteParams m_quoteParams;
    TransformerParams m_params;
    CalcInput m_input;
    CalcResult m_result;
    QuoteResult m_quote;
    bool m_hasScheme = false;
};

#endif // QUOTEPAGE_H
