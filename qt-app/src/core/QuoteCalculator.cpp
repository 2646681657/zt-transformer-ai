#include "QuoteCalculator.h"

#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <cmath>

// ============================================================================
// QuoteParams 序列化
// ============================================================================

QJsonObject QuoteParams::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("steelPrice")] = steelPrice;
    o[QStringLiteral("cuPrice")] = cuPrice;
    o[QStringLiteral("alPrice")] = alPrice;
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
    p.alPrice = o[QStringLiteral("alPrice")].toDouble(p.alPrice);
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

// 直接取计算结果中的净重，再按计算单的采购系数换算毛重；
// 不从成本反推重量，避免切换材料单价时改变数量。
QuoteResult QuoteCalculator::calculate(const TransformerParams &params,
                                        const CalcInput &input,
                                        const CalcResult &r,
                                        const QuoteParams &q)
{
    QuoteResult out;
    if (!r.valid) {
        return out;
    }

    // ---- 材料项：按报价单价重算（用量沿用引擎口径）----
    const double steelW = std::round(r.core.coreWeight_kg * 1.05);
    const double hvW = std::round(r.winding.hvWireWeight_kg * 1.05);
    const double lvW = std::round(r.winding.lvWireWeight_kg * 1.08);
    const double oilW = std::round(r.mass.oilWeight_kg * 1.1);
    const double tankW = std::round(r.mass.tankWeight_kg * 1.05) + 200.0;

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
    const double hvPrice = input.hvCopperWire ? q.cuPrice + 3.3
        : q.alPrice + (input.hvBareWidth_mm == input.hvBareThick_mm ? 6.0 : 8.5);
    const double lvPrice = input.lvCopperFoil ? q.cuPrice * 1.05 + 6.5
        : q.alPrice + 6.0;
    addMaterial(input.hvCopperWire ? QStringLiteral("高压铜导线") : QStringLiteral("高压铝导线"),
                hvW, hvPrice);
    addMaterial(input.lvCopperFoil ? QStringLiteral("低压铜箔") : QStringLiteral("低压铝箔"),
                lvW, lvPrice);
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
