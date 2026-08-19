#ifndef SCHEMECONSTRAINTS_H
#define SCHEMECONSTRAINTS_H
// 方案约束校验：按性能标准值 + 允许偏差过滤不合格方案
// （空载/负载/总损耗、阻抗电压、空载电流、温升限值；
//   标准值 ≤0 视为该项不校验）

#include <QString>
#include <QStringList>
#include "TransformerParams.h"
#include "CalcResult.h"

struct SchemeConstraintsResult {
    bool passed = true;
    QStringList violations;   // 未通过项描述
};

inline SchemeConstraintsResult checkSchemeConstraints(const TransformerParams &p,
                                                       const CalcResult &r)
{
    SchemeConstraintsResult ret;

    // 上限校验：实际值 ≤ 标准值 × (1 + 偏差%)
    const auto upper = [&ret](const QString &name, double actual, double stdVal,
                              double devPct) {
        if (stdVal <= 0.0) {
            return;   // 标准值未填：跳过该项
        }
        const double limit = stdVal * (1.0 + devPct / 100.0);
        if (actual > limit) {
            ret.passed = false;
            ret.violations << QStringLiteral("%1 %2 超出限值 %3")
                                  .arg(name)
                                  .arg(QString::number(actual, 'f', 1))
                                  .arg(QString::number(limit, 'f', 1));
        }
    };

    upper(QStringLiteral("空载损耗"), r.core.noLoadLoss_W,
          p.noLoadLossStd_W, p.noLoadLossMaxDev_pct);
    upper(QStringLiteral("负载损耗"), r.winding.loadLoss_W,
          p.loadLossStd_W, p.loadLossMaxDev_pct);
    upper(QStringLiteral("总损耗"), r.core.noLoadLoss_W + r.winding.loadLoss_W,
          p.totalLossStd_W, p.totalLossMaxDev_pct);
    upper(QStringLiteral("空载电流%"), r.core.noLoadCurrent_pct,
          p.noLoadCurrentStd_pct, p.noLoadCurrentMaxDev_pct);
    upper(QStringLiteral("油顶层温升K"), r.thermal.oilTopRise_K, p.oilTopTempRise_K, 0.0);
    upper(QStringLiteral("高压绕组温升K"), r.thermal.hvWindingRise_K, p.hvCoilTempRise_K, 0.0);
    upper(QStringLiteral("低压绕组温升K"), r.thermal.lvWindingRise_K, p.lvCoilTempRise_K, 0.0);

    // 阻抗电压：区间 [标准值×(1+最小偏差), 标准值×(1+最大偏差)]
    if (p.impedanceVoltageStd_pct > 0.0) {
        const double lo = p.impedanceVoltageStd_pct
                          * (1.0 + p.impedanceVoltageMinDev_pct / 100.0);
        const double hi = p.impedanceVoltageStd_pct
                          * (1.0 + p.impedanceVoltageMaxDev_pct / 100.0);
        if (r.impedance.impedance_pct < lo || r.impedance.impedance_pct > hi) {
            ret.passed = false;
            ret.violations << QStringLiteral("阻抗电压%1 超出范围 [%2, %3]")
                                  .arg(QString::number(r.impedance.impedance_pct, 'f', 2))
                                  .arg(QString::number(lo, 'f', 2))
                                  .arg(QString::number(hi, 'f', 2));
        }
    }
    return ret;
}

#endif // SCHEMECONSTRAINTS_H
