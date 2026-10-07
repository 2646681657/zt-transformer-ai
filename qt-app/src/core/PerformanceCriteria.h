#ifndef PERFORMANCECRITERIA_H
#define PERFORMANCECRITERIA_H
#include "TransformerParams.h"
#include <QRegularExpression>
#include <cmath>

namespace PerformanceCriteria {
inline QString valueText(double value)
{
    // 常用小数保持简洁；必要时使用17位保证保存/恢复后数值不变。
    const QString compact = QString::number(value, 'g', 15);
    return compact.toDouble() == value ? compact : QString::number(value, 'g', 17);
}
inline bool validModel(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^([A-Za-z0-9]+-M)-([0-9]+(?:\\.[0-9]+)?)/([0-9]+(?:\\.[0-9]+)?)-([0-9]+(?:\\.[0-9]+)?)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(text.trimmed());
    if (!match.hasMatch()) return false;
    for (int i = 2; i <= 4; ++i) {
        bool ok = false;
        const double value = match.captured(i).toDouble(&ok);
        if (!ok || !std::isfinite(value) || value <= 0.0) return false;
    }
    return true;
}
struct Field { const char *key; double TransformerParams::*member; };
inline const Field *fields(int &count)
{
    static const Field values[] = {
        {"noLoadLossStd_W", &TransformerParams::noLoadLossStd_W},
        {"noLoadLossMaxDev_pct", &TransformerParams::noLoadLossMaxDev_pct},
        {"loadLossStd_W", &TransformerParams::loadLossStd_W},
        {"loadLossMaxDev_pct", &TransformerParams::loadLossMaxDev_pct},
        {"totalLossStd_W", &TransformerParams::totalLossStd_W},
        {"totalLossMaxDev_pct", &TransformerParams::totalLossMaxDev_pct},
        {"impedanceVoltageStd_pct", &TransformerParams::impedanceVoltageStd_pct},
        {"impedanceVoltageMaxDev_pct", &TransformerParams::impedanceVoltageMaxDev_pct},
        {"impedanceVoltageMinDev_pct", &TransformerParams::impedanceVoltageMinDev_pct},
        {"noLoadCurrentStd_pct", &TransformerParams::noLoadCurrentStd_pct},
        {"noLoadCurrentMaxDev_pct", &TransformerParams::noLoadCurrentMaxDev_pct},
        {"oilTopTempRise_K", &TransformerParams::oilTopTempRise_K},
        {"hvCoilTempRise_K", &TransformerParams::hvCoilTempRise_K},
        {"lvCoilTempRise_K", &TransformerParams::lvCoilTempRise_K}
    };
    count = sizeof(values) / sizeof(values[0]);
    return values;
}
inline void restore(const TransformerParams &saved, TransformerParams &target)
{
    int count;
    const auto *list = fields(count);
    for (int i = 0; i < count; ++i) target.*(list[i].member) = saved.*(list[i].member);
    target.standardMode = saved.standardMode;
    target.lossStandardsManual = saved.lossStandardsManual;
    target.lossStandardsKey = saved.lossStandardsKey;
    target.productModel = saved.productModel;
}
}
#endif
