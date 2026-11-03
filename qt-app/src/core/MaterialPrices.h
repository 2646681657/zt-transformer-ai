#ifndef MATERIALPRICES_H
#define MATERIALPRICES_H

#include <QStringList>
#include <array>
#include <cmath>
#include <limits>

// 寻优材料基价快照。导线加工加价仍由计算单公式处理，不是成品导线价。
struct MaterialPrices {
    bool custom = false;
    double copper = 60.0;
    double aluminum = 60.0;
    double oil = 10.0;
    double tank = 9.0;

    MaterialPrices effective() const { return custom ? *this : MaterialPrices{}; }
    QString validationError() const {
        if (!custom) return {};
        const std::array<double, 4> prices{copper, aluminum, oil, tank};
        const QStringList names{QStringLiteral("铜"), QStringLiteral("铝"),
                                QStringLiteral("绝缘油"), QStringLiteral("油箱钢材")};
        for (int i = 0; i < 4; ++i)
            if (!std::isfinite(prices[i]) || prices[i] <= 0.0 || prices[i] > 99999.0)
                return QStringLiteral("%1自定义基价须大于0、不超过99999元/kg；四项均须填写有效数值，不自动回退为内置价或报价页价格。")
                    .arg(names[i]);
        return {};
    }
    static MaterialPrices fromTexts(bool enabled, const QStringList &texts) {
        MaterialPrices p;
        p.custom = enabled;
        double *values[]{&p.copper, &p.aluminum, &p.oil, &p.tank};
        for (int i = 0; i < 4; ++i) {
            bool ok = false;
            const double value = texts.value(i).trimmed().toDouble(&ok);
            *values[i] = ok ? value : std::numeric_limits<double>::quiet_NaN();
        }
        return p;
    }
    QString description() const {
        const auto p = effective();
        return (custom ? QStringLiteral("其他材料自定义基价：") : QStringLiteral("其他材料内置基价："))
            + QStringLiteral("铜%1、铝%2、油%3、油箱钢材%4元/kg；导线加工加价、计价重量系数及舍入沿用计算单。")
                .arg(p.copper, 0, 'g', 15).arg(p.aluminum, 0, 'g', 15)
                .arg(p.oil, 0, 'g', 15).arg(p.tank, 0, 'g', 15);
    }
};

#endif // MATERIALPRICES_H
