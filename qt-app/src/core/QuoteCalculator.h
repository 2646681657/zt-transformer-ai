#ifndef QUOTECALCULATOR_H
#define QUOTECALCULATOR_H
// 产品报价：基于电磁计算的材料用量与成本结果，叠加可调单价与
// 费用系数（外购件/人工/管理费/利润率）生成报价明细

#include <QString>
#include <QVector>
#include <QJsonObject>
#include "CalcResult.h"
#include "TransformerParams.h"

// 报价参数（材料单价 + 费用系数，可随行情调整并持久化）
struct QuoteParams {
    // 材料单价（元/kg 或元/台，默认值对齐引擎内置价格）
    double steelPrice = 17.0;         // 硅钢片 元/kg
    double cuPrice = 60.0;            // 铜导线基价 元/kg
    double oilPrice = 10.0;           // 绝缘油 元/kg
    double tankPrice = 9.0;           // 油箱钢材 元/kg
    // 费用系数（占材料成本百分比）
    double purchasedParts_pct = 8.0;  // 外购件
    double labor_pct = 10.0;          // 人工
    double management_pct = 5.0;      // 管理费
    double profit_pct = 15.0;         // 利润率
    // 税率（增值税，报价含税）
    double tax_pct = 13.0;
    // 其他固定费用（元/台）
    double miscCost = 200.0;

    QJsonObject toJson() const;
    static QuoteParams fromJson(const QJsonObject &o);
    // 持久化到用户配置目录（失败返回 false）
    static bool saveToFile(const QString &path, const QuoteParams &p);
    static QuoteParams loadFromFile(const QString &path, bool *ok = nullptr);
};

// 报价明细行（材料项或费用项）
struct QuoteLine {
    QString name;        // 项目名称
    double quantity = 0.0;  // 数量（kg 或百分比）
    QString unit;        // 单位
    double unitPrice = 0.0; // 单价/百分比
    double amount = 0.0;   // 金额（元）
    bool isFee = false;  // true=费用项（按系数计算），false=材料项
};

// 报价结果
struct QuoteResult {
    QVector<QuoteLine> lines;       // 明细行
    double materialCost = 0.0;      // 材料成本小计
    double feeCost = 0.0;           // 费用小计（外购件+人工+管理）
    double costTotal = 0.0;         // 成本合计
    double profit = 0.0;            // 利润
    double tax = 0.0;               // 税额
    double quotePrice = 0.0;        // 含税出厂报价
    bool valid = false;
};

namespace QuoteCalculator {
// 用报价参数重算材料成本并叠加费用：结果需有效（result.valid）
QuoteResult calculate(const TransformerParams &params, const CalcResult &r,
                      const QuoteParams &q);
// 报价参数默认持久化路径（用户 AppData 配置目录）
QString defaultParamsPath();
}

#endif // QUOTECALCULATOR_H
