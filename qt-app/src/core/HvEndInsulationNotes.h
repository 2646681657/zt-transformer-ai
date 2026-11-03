#ifndef HVENDINSULATIONNOTES_H
#define HVENDINSULATIONNOTES_H

#include "CalcResult.h"
#include <cmath>

// 只读结果快照的展示提示；不改变公式、输入、计算有效性或寻优约束。
namespace HvEndInsulationNotes {

inline bool available(const WindingResult &w)
{
    return std::isfinite(w.hvEndInsul_mm)
        && std::isfinite(w.hvEndInsulReference_mm)
        && w.hvEndInsulReference_mm > 0.0
        && std::isfinite(w.hvEndInsul_mm - w.hvEndInsulReference_mm);
}

inline QString marginText(const WindingResult &w)
{
    if (!available(w)) return QStringLiteral("不可用");
    // 保留有效数字，避免微小负差额被两位小数显示成0；判读使用未格式化值。
    return QString::number(w.hvEndInsul_mm - w.hvEndInsulReference_mm, 'g', 15);
}

inline QString status(const WindingResult &w)
{
    if (!available(w)) return QStringLiteral("需核对数据");
    if (w.hvEndInsul_mm < 0.0) return QStringLiteral("端绝缘为负");
    if (w.hvEndInsul_mm < w.hvEndInsulReference_mm) return QStringLiteral("低于参考值");
    if (w.hvEndInsul_mm == w.hvEndInsulReference_mm) return QStringLiteral("等于参考值");
    return QStringLiteral("高于参考值");
}

inline QString note(const WindingResult &w)
{
    if (!available(w)) {
        return QStringLiteral("端绝缘或参考下限数据不可用，请人工核对；不作合格判定。");
    }
    if (w.hvEndInsul_mm < w.hvEndInsulReference_mm) {
        const QString shortage = QString::number(w.hvEndInsulReference_mm - w.hvEndInsul_mm, 'g', 15);
        return (w.hvEndInsul_mm < 0.0 ? QStringLiteral("端绝缘为负；") : QString())
            + QStringLiteral("低于原计算单参考下限 %1 mm，请人工核对；未参与合格判定或寻优筛选。")
                .arg(shortage);
    }
    return QStringLiteral("未低于原计算单参考下限；这不代表绝缘合格，仍需人工核对；未参与合格判定或寻优筛选。");
}

} // namespace HvEndInsulationNotes

#endif // HVENDINSULATIONNOTES_H
