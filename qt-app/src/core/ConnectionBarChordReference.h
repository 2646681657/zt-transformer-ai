#ifndef CONNECTIONBARCHORDREFERENCE_H
#define CONNECTIONBARCHORDREFERENCE_H

#include "CalcInput.h"
#include "CalcResult.h"
#include <algorithm>
#include <cmath>

// 原单AN14→线圈型式Y17/Z20:Z35→K19/K20，只生成独立显示快照。
namespace ConnectionBarChordReference {
inline void capture(const CalcInput &in, CalcResult &r)
{
    if (!r.valid || !r.sheetDetailsSnapshotAvailable) return;
    for (const char *a : {"AN14", "K19", "K20"}) {
        const QString key = QString::fromLatin1(a);
        r.calculationSheetDetails.remove(key);
        r.calculationSheetTexts.remove(key);
        r.calculationSheetNotes.remove(key);
    }
    const QString scope = QStringLiteral("仅当前椭圆形内连接排查表参考，未参与电磁、温升、成本、整机合格或寻优筛选；不代替制造安全核对。原表弦高固定扣减4mm的工艺依据待确认。");
    const auto unavailable = [&](const char *address, const QString &status, const QString &reason) {
        const QString key = QString::fromLatin1(address);
        r.calculationSheetTexts.insert(key, status);
        r.calculationSheetNotes.insert(key, reason + QLatin1Char('\n') + scope);
    };
    const auto allUnavailable = [&](const QString &status, const QString &reason) {
        for (const char *a : {"AN14", "K19", "K20"}) unavailable(a, status, reason);
    };
    const QString thickness = in.lvConnectionBarThicknessText.trimmed();
    const QString width = in.lvConnectionBarWidthText.trimmed();
    if (thickness.isEmpty() || width.isEmpty()) {
        allUnavailable(thickness.isEmpty() && width.isEmpty() ? QStringLiteral("未录入") : QStringLiteral("待补全"),
            QStringLiteral("请先填写原单第12行AG12厚度、AJ12宽度，不使用示例尺寸。"));
        return;
    }
    bool thicknessOk = false, widthOk = false;
    const double t = thickness.toDouble(&thicknessOk);
    const double w = width.toDouble(&widthOk);
    if (!thicknessOk || !widthOk || !std::isfinite(t) || !std::isfinite(w) || t <= 0.0 || w <= 0.0) {
        allUnavailable(QStringLiteral("不可用"), QStringLiteral("内排厚度、宽度须为大于0的有限数值。"));
        return;
    }
    const double radius = in.coreDiameter_mm / 2.0 + in.phaseGapBase_mm;
    const double radicand = radius * radius - (w / 2.0) * (w / 2.0);
    if (!std::isfinite(radius) || !std::isfinite(radicand) || radicand < 0.0) {
        allUnavailable(QStringLiteral("不可用"), QStringLiteral("弦高根号内数值为负或非有限，不能计算；未钳位为0。"));
        return;
    }
    // Excel ROUND(...,0)：先计算完整表达式，再按half-away-from-zero取整。
    const double chord = std::round(std::sqrt(radicand) - t - 4.0 + in.coreStraight_mm / 2.0);
    if (!std::isfinite(chord)) {
        allUnavailable(QStringLiteral("不可用"), QStringLiteral("弦高结果不是有限数值。"));
        return;
    }
    r.calculationSheetDetails.insert(QStringLiteral("AN14"), {chord, 0});
    r.calculationSheetNotes.insert(QStringLiteral("AN14"),
        QStringLiteral("ROUND(SQRT((H12/2+B28)²−(AJ12/2)²)−AG12−4+K12/2,0)。当前H12=%1、B28=%2、AG12=%3、AJ12=%4、K12=%5；先完整计算再取整。")
            .arg(QString::number(in.coreDiameter_mm, 'g', 15), QString::number(in.phaseGapBase_mm, 'g', 15),
                thickness, width, QString::number(in.coreStraight_mm, 'g', 15)) + QLatin1Char('\n') + scope);

    const auto lookupUnavailable = [&](const QString &reason) {
        for (const char *a : {"K19", "K20"}) unavailable(a, QStringLiteral("不可用"), reason);
    };
    if (r.core.widths_mm.size() < 15 || r.core.stacks_mm.size() < 15) {
        lookupUnavailable(QStringLiteral("缺少原单15级片宽/几何叠厚快照，不能构造线圈型式查表。"));
        return;
    }
    // 椭圆形时原表第20行宽/厚均为0；21..35行=原单G17:S17、F19:I19及对应几何叠厚。
    double widths[16] = {0.0};
    double cumulative[16] = {0.0};
    for (int i = 0; i < 15; ++i) {
        const double pieceWidth = r.core.widths_mm[i];
        const double stack = r.core.stacks_mm[i];
        if (!std::isfinite(pieceWidth) || !std::isfinite(stack) || pieceWidth < 0.0 || stack < 0.0) {
            lookupUnavailable(QStringLiteral("片宽/几何叠厚非有限或为负，累计表不满足原LOOKUP升序前提；不排序、不钳位。"));
            return;
        }
        widths[i+1] = pieceWidth;
        cumulative[i+1] = cumulative[i] + stack;
        if (!std::isfinite(cumulative[i+1])) {
            lookupUnavailable(QStringLiteral("累计几何叠厚超出有效数值范围。"));
            return;
        }
    }
    // LOOKUP取不大于弦高的最后一行；累计厚重复时必须取最后匹配，不能取第一个。
    int matched = -1;
    for (int i = 0; i < 16; ++i) if (cumulative[i] <= chord) matched = i;
    if (matched < 0) {
        lookupUnavailable(QStringLiteral("弦高小于累计叠厚首项0，原LOOKUP无匹配；不以首行或0回退。"));
        return;
    }
    // 原K19在匹配序号+1后再次近似LOOKUP，超过序号16仍返回末行，不外推新片宽。
    const int next = std::min(matched + 1, 15);
    const double remainder = chord - cumulative[matched];
    if (!std::isfinite(remainder)) {
        lookupUnavailable(QStringLiteral("弦高相对累计叠厚余量不是有限数值。"));
        return;
    }
    r.calculationSheetDetails.insert(QStringLiteral("K19"), {widths[next], 2});
    r.calculationSheetDetails.insert(QStringLiteral("K20"), {remainder, 2});
    const QString lookupNote = QStringLiteral("线圈型式表第%1行命中，累计叠厚%2mm；K19读取序号+1后的近似匹配片宽，K20=弦高−命中累计叠厚。重复累计值取最后一行；越过末序号按原LOOKUP返回末行，0片宽保留为真实表值，不表示制造合格。")
        .arg(20 + matched).arg(QString::number(cumulative[matched], 'g', 15));
    for (const char *a : {"K19", "K20"})
        r.calculationSheetNotes.insert(QString::fromLatin1(a), lookupNote + QLatin1Char('\n') + scope);
}
} // namespace ConnectionBarChordReference

#endif
