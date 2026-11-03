#ifndef SCHEMECONSTRAINTS_H
#define SCHEMECONSTRAINTS_H
// 方案约束校验：按性能标准值 + 允许偏差过滤不合格方案
// （空载/负载/总损耗、阻抗电压、空载电流、温升限值；
//   标准值 ≤0 视为该项不校验）

#include <QString>
#include <QStringList>
#include <QVector>
#include <cmath>
#include "TransformerParams.h"
#include "CalcResult.h"

struct SchemeConstraintMargin {
    QString name;
    QString unit;
    bool enabled = false;
    bool hasLower = false;
    double actual = 0.0;
    double lower = 0.0;
    double upper = 0.0;
};

struct SchemeConstraintsResult {
    bool passed = true;
    QStringList violations;   // 未通过项描述
    QStringList failedChecks; // 稳定的失败项名称，用于统计（同一组合可能多项失败）
    QStringList skippedChecks;
    QVector<SchemeConstraintMargin> margins; // 与筛选共用实际值及限值，只作观察
};

inline SchemeConstraintsResult checkSchemeConstraints(const TransformerParams &p,
                                                       const CalcResult &r)
{
    SchemeConstraintsResult ret;

    // 上限校验：实际值 ≤ 标准值 × (1 + 偏差%)
    const auto upper = [&ret](const QString &name, const QString &unit, double actual, double stdVal,
                              double devPct) {
        if (stdVal <= 0.0) {
            ret.margins.append({name, unit, false});
            ret.skippedChecks << name;
            return;   // 标准值未填：跳过该项
        }
        const double limit = stdVal * (1.0 + devPct / 100.0);
        ret.margins.append({name, unit, true, false, actual, 0.0, limit});
        if (actual > limit) {
            ret.passed = false;
            ret.failedChecks << name;
            ret.violations << QStringLiteral("%1 %2 超出限值 %3")
                                  .arg(name)
                                  .arg(QString::number(actual, 'f', 1))
                                  .arg(QString::number(limit, 'f', 1));
        }
    };

    upper(QStringLiteral("空载损耗"), QStringLiteral("W"), r.core.noLoadLoss_W,
          p.noLoadLossStd_W, p.noLoadLossMaxDev_pct);
    upper(QStringLiteral("负载损耗"), QStringLiteral("W"), r.winding.loadLoss_W,
          p.loadLossStd_W, p.loadLossMaxDev_pct);
    upper(QStringLiteral("总损耗"), QStringLiteral("W"), r.core.noLoadLoss_W + r.winding.loadLoss_W,
          p.totalLossStd_W, p.totalLossMaxDev_pct);
    upper(QStringLiteral("空载电流%"), QStringLiteral("%"), r.core.noLoadCurrent_pct,
          p.noLoadCurrentStd_pct, p.noLoadCurrentMaxDev_pct);
    upper(QStringLiteral("油顶层温升K"), QStringLiteral("K"), r.thermal.oilTopRise_K, p.oilTopTempRise_K, 0.0);
    upper(QStringLiteral("高压绕组温升K"), QStringLiteral("K"), r.thermal.hvWindingRise_K, p.hvCoilTempRise_K, 0.0);
    upper(QStringLiteral("低压绕组温升K"), QStringLiteral("K"), r.thermal.lvWindingRise_K, p.lvCoilTempRise_K, 0.0);

    // 阻抗电压：区间 [标准值×(1+最小偏差), 标准值×(1+最大偏差)]
    if (p.impedanceVoltageStd_pct > 0.0) {
        const double lo = p.impedanceVoltageStd_pct
                          * (1.0 + p.impedanceVoltageMinDev_pct / 100.0);
        const double hi = p.impedanceVoltageStd_pct
                          * (1.0 + p.impedanceVoltageMaxDev_pct / 100.0);
        ret.margins.append({QStringLiteral("阻抗电压%"), QStringLiteral("%"), true, true,
                            r.impedance.impedance_pct, lo, hi});
        if (r.impedance.impedance_pct < lo || r.impedance.impedance_pct > hi) {
            ret.passed = false;
            ret.failedChecks << QStringLiteral("阻抗电压%");
            ret.violations << QStringLiteral("阻抗电压%1 超出范围 [%2, %3]")
                                  .arg(QString::number(r.impedance.impedance_pct, 'f', 2))
                                  .arg(QString::number(lo, 'f', 2))
                                  .arg(QString::number(hi, 'f', 2));
        }
    } else {
        ret.margins.append({QStringLiteral("阻抗电压%"), QStringLiteral("%"), false});
        ret.skippedChecks << QStringLiteral("阻抗电压%");
    }
    return ret;
}

inline QStringList schemeConstraintMarginText(const SchemeConstraintsResult &checks)
{
    QStringList rows;
    const auto value = [](double v) { return QString::number(v, 'g', 12); };
    for (const auto &m : checks.margins) {
        if (!m.enabled) {
            rows.append(m.name + QStringLiteral("：未校核（标准值≤0），不报告裕量。"));
            continue;
        }
        if (!std::isfinite(m.actual) || !std::isfinite(m.upper) || m.upper <= 0.0
                || (m.hasLower && (!std::isfinite(m.lower) || m.lower < 0.0 || m.lower > m.upper))) {
            rows.append(m.name + QStringLiteral("：实际值或限值无效，无法报告裕量。"));
            continue;
        }
        const QString marginUnit = m.unit == QStringLiteral("%") ? QStringLiteral("个百分点") : m.unit;
        const double upperMargin = m.upper - m.actual;
        const double lowerMargin = m.actual - m.lower;
        if (!std::isfinite(upperMargin) || (m.hasLower && !std::isfinite(lowerMargin))) {
            rows.append(m.name + QStringLiteral("：差值无效，无法报告裕量。"));
            continue;
        }
        if (m.hasLower) {
            rows.append(QStringLiteral("%1：实际%2 %3；允许范围[%4, %5] %3；距下限%6、距上限%7 %8。")
                .arg(m.name, value(m.actual), m.unit, value(m.lower), value(m.upper),
                     value(lowerMargin), value(upperMargin), marginUnit));
        } else {
            rows.append(QStringLiteral("%1：实际%2 %3；允许上限%4 %3；剩余裕量%5 %6。")
                .arg(m.name, value(m.actual), m.unit, value(m.upper), value(upperMargin), marginUnit));
        }
    }
    return rows;
}

#endif // SCHEMECONSTRAINTS_H
