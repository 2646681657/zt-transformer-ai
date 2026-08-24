#include "QuoteCalculator.h"

#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>

// ============================================================================
// QuoteParams 序列化
// ============================================================================

QJsonObject QuoteParams::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("steelPrice")] = steelPrice;
    o[QStringLiteral("cuPrice")] = cuPrice;
    o[QStringLiteral("oilPrice")] = oilPrice;
    o[QStringLiteral("tankPrice")] = tankPrice;
    o[QStringLiteral("purchasedParts_pct")] = purchasedParts_pct;
    o[QStringLiteral("labor_pct")] = labor_pct;
    o[QStringLiteral("management_pct")] = management_pct;
    o[QStringLiteral("profit_pct")] = profit_pct;
    o[QStringLiteral("tax_pct")] = tax_pct;
    o[QStringLiteral("miscCost")] = miscCost;
    return o;
}

QuoteParams QuoteParams::fromJson(const QJsonObject &o)
{
    QuoteParams p;
    p.steelPrice = o[QStringLiteral("steelPrice")].toDouble(p.steelPrice);
    p.cuPrice = o[QStringLiteral("cuPrice")].toDouble(p.cuPrice);
    p.oilPrice = o[QStringLiteral("oilPrice")].toDouble(p.oilPrice);
    p.tankPrice = o[QStringLiteral("tankPrice")].toDouble(p.tankPrice);
    p.purchasedParts_pct = o[QStringLiteral("purchasedParts_pct")].toDouble(p.purchasedParts_pct);
    p.labor_pct = o[QStringLiteral("labor_pct")].toDouble(p.labor_pct);
    p.management_pct = o[QStringLiteral("management_pct")].toDouble(p.management_pct);
    p.profit_pct = o[QStringLiteral("profit_pct")].toDouble(p.profit_pct);
    p.tax_pct = o[QStringLiteral("tax_pct")].toDouble(p.tax_pct);
    p.miscCost = o[QStringLiteral("miscCost")].toDouble(p.miscCost);
    return p;
}

bool QuoteParams::saveToFile(const QString &path, const QuoteParams &p)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    QJsonObject root;
    root[QStringLiteral("quoteParams")] = p.toJson();
    root[QStringLiteral("savedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QJsonDocument doc(root);
    return f.write(doc.toJson(QJsonDocument::Indented)) > 0;
}

QuoteParams QuoteParams::loadFromFile(const QString &path, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QuoteParams();
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return QuoteParams();
    }
    if (ok) {
        *ok = true;
    }
    return QuoteParams::fromJson(
        doc.object()[QStringLiteral("quoteParams")].toObject());
}

// ============================================================================
// 报价计算
// ============================================================================

QString QuoteCalculator::defaultParamsPath()
{
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/quote_params.json");
}

// 材料重量从引擎成本反推：成本单价随报价参数变化，重量（含损耗系数
// 前的净重不可拆分）直接用引擎已含 1.05/1.08/1.1 损耗的毛重口径，
// 报价单价只调整价格部分，保持与计算单成本同口径可对比
QuoteResult QuoteCalculator::calculate(const TransformerParams &params,
                                        const CalcResult &r,
                                        const QuoteParams &q)
{
    QuoteResult out;
    if (!r.valid) {
        return out;
    }

    // ---- 材料项：按报价单价重算（用量沿用引擎口径）----
    // 硅钢：steelCost/17 反推毛重
    const double steelW = r.cost.steelCost > 0.0 ? r.cost.steelCost / 17.0 : 0.0;
    const double hvW = 63.3 > 0.0 && r.cost.hvWireCost > 0.0
                           ? r.cost.hvWireCost / 63.3 : 0.0;          // cuPrice+3.3
    const double lvW = r.cost.lvWireCost > 0.0
                           ? r.cost.lvWireCost / (60.0 * 1.05 + 6.5) : 0.0;
    const double oilW = r.cost.oilCost > 0.0 ? r.cost.oilCost / 10.0 : 0.0;
    const double tankW = r.cost.tankCost > 0.0
                             ? (r.cost.tankCost / 9.0 - 200.0) : 0.0;

    auto addMaterial = [&out](const QString &name, double w, double price) {
        QuoteLine line;
        line.name = name;
        line.quantity = w;
        line.unit = QStringLiteral("kg");
        line.unitPrice = price;
        line.amount = w * price;
        out.lines.append(line);
        out.materialCost += line.amount;
    };

    addMaterial(QStringLiteral("硅钢片"), steelW, q.steelPrice);
    addMaterial(QStringLiteral("高压导线"), hvW, q.cuPrice + 3.3);
    addMaterial(QStringLiteral("低压箔"), lvW, q.cuPrice * 1.05 + 6.5);
    addMaterial(QStringLiteral("绝缘油"), oilW, q.oilPrice);
    addMaterial(QStringLiteral("油箱及结构件"), tankW, q.tankPrice);

    // ---- 费用项：按材料成本百分比 ----
    auto addFee = [&out](const QString &name, double base, double pct) {
        QuoteLine line;
        line.name = name;
        line.quantity = pct;
        line.unit = QStringLiteral("%");
        line.unitPrice = pct;
        line.amount = base * pct / 100.0;
        line.isFee = true;
        out.lines.append(line);
        out.feeCost += line.amount;
    };

    const double feeBase = out.materialCost;
    addFee(QStringLiteral("外购件"), feeBase, q.purchasedParts_pct);
    addFee(QStringLiteral("人工"), feeBase, q.labor_pct);
    addFee(QStringLiteral("管理费"), feeBase, q.management_pct);

    // 其他固定费用
    {
        QuoteLine line;
        line.name = QStringLiteral("其他费用");
        line.quantity = 1.0;
        line.unit = QStringLiteral("台");
        line.unitPrice = q.miscCost;
        line.amount = q.miscCost;
        line.isFee = true;
        out.lines.append(line);
        out.feeCost += q.miscCost;
    }

    // ---- 合计 ----
    out.costTotal = out.materialCost + out.feeCost;
    out.profit = out.costTotal * q.profit_pct / 100.0;
    const double pretax = out.costTotal + out.profit;
    out.tax = pretax * q.tax_pct / 100.0;
    out.quotePrice = pretax + out.tax;
    out.valid = true;

    // 备注型号（未使用 params 时消除告警）
    Q_UNUSED(params);
    return out;
}
