#ifndef CALCULATIONSHEETDESCRIPTIONS_H
#define CALCULATIONSHEETDESCRIPTIONS_H

#include "CalcInput.h"
#include "CalcResult.h"
#include <QStringList>
#include <algorithm>

// 显示层说明：只读取本次输入/结果，不求解、不写回设计变量或筛选条件。
namespace CalculationSheetDescriptions {
inline QStringList addresses()
{
    return QStringLiteral(
        "X8 Z8 Z9 X10 Z10 Z11 Z15 W23 AA23 AJ25 AK25 AC33 B46 "
        "L12 L19 M19 O19 R19 Q44 Q45 Q46 Q47 Q48 Q49 Q50 Q51 "
        "E27 F27 Q52 Q53 E29 E30 E31 E32 E33 E34 E35 E36 E37 E38 "
        "E39 E40 E41 E42 E43 H44 E45 E46 E47 E49 J49 H51 E50 "
        "U48 AD48 U49 AD49 U50 U51 U52 AD50 E52 E53 E54 I53 H54 O53 O54 P54 P49 AA53 "
        "AF33 AF34 W36 W39 AF36 AF39")
        .split(QLatin1Char(' '));
}

inline void capture(const CalcInput &in, CalcResult &r)
{
    if (!r.valid || !r.sheetDetailsSnapshotAvailable) return;
    const auto number = [](double value) { return QString::number(value, 'g', 12); };
    const auto detail = [&](const char *address) {
        const auto it = r.calculationSheetDetails.constFind(QString::fromLatin1(address));
        return it == r.calculationSheetDetails.cend() ? QStringLiteral("未接入") : number(it.value().first);
    };
    const auto text = [&](const char *address, const QString &value, const QString &note = QString()) {
        const QString key = QString::fromLatin1(address);
        r.calculationSheetTexts.insert(key, value); // 空字符串也是明确的原表条件分支。
        r.calculationSheetNotes.insert(key, note.isEmpty()
            ? QStringLiteral("原计算单说明文字；只读，不作为新增输入或合格判定。") : note);
    };
    text("X8", QStringLiteral("(%1)").arg(r.winding.hvTurnsMax - r.winding.hvTurnsRated));
    text("Z8", QStringLiteral("(%1)").arg(r.winding.hvTurnsRated - r.winding.hvTurnsMin));
    for (const char *a : {"Z9", "Z11", "Z15"}) text(a, QStringLiteral("="));
    // 原表比较的是空字符串，数值0仍应显示乘号/等号，不能按大于0判断。
    const bool secondSegment = r.calculationSheetDetails.contains(QStringLiteral("W10"));
    text("X10", secondSegment ? QStringLiteral("×") : QString());
    text("Z10", secondSegment ? QStringLiteral("=") : QString());
    text("W23", QStringLiteral("× %1").arg(in.hvTurnsPerLayer));
    text("AA23", QStringLiteral("× %1").arg(r.winding.layerCount + 1));
    text("AJ25", QStringLiteral("= %1").arg(number(r.winding.lvAxial_mm)));
    text("AK25", QStringLiteral("-Hw"));
    text("AC33", QStringLiteral("-Hw"));
    text("B46", QStringLiteral("× 2"));
    text("L12", QStringLiteral("椭圆角"));
    text("L19", QStringLiteral("短轴长"));
    text("M19", QStringLiteral("大圆半径"));
    text("O19", QStringLiteral("圆心到轴"));
    text("R19", QStringLiteral("交接点高"));
    const char *waveAddresses[] = {"Q44", "Q45", "Q46", "Q47", "Q48", "Q49", "Q50", "Q51"};
    const QStringList waveLabels = {QStringLiteral("波纹厚"), QStringLiteral("波纹深"),
        QStringLiteral("波纹高"), QStringLiteral("波纹节距"), QStringLiteral("波纹长边"),
        QStringLiteral("波纹短边"), QStringLiteral("Ks ="), QStringLiteral("Kp =")};
    for (int i = 0; i < waveLabels.size(); ++i) text(waveAddresses[i], waveLabels[i]);
    for (const char *a : {"E27", "F27", "Q52", "Q53"})
        text(a, QString(), QStringLiteral("原表仅片散油箱分支显示；当前支持波纹油箱，此处按原表留空，不代表储油柜/片散算法已接入。"));

    // 只展开本次平均宽/长数值公式的操作数，不在说明层重新求解几何结果。
    double lvDuctHeightSum = 0.0, lvDuctWidthSum = 0.0, hvDuctHeightSum = 0.0;
    double lvFirstThreeWidths = 0.0, hvFirstThreeWidths = 0.0;
    for (int i = 0; i < 5; ++i) {
        lvDuctHeightSum += in.lvDuctHeightSide[i];
        lvDuctWidthSum += in.lvDuctWidthSide[i];
        hvDuctHeightSum += in.hvDuctHeightSide[i];
        if (i < 3) {
            lvFirstThreeWidths += in.lvDuctWidthSide[i];
            hvFirstThreeWidths += in.hvDuctWidthSide[i];
        }
    }
    const QString diameter = number(in.coreDiameter_mm);
    const QString phaseGap = number(in.phaseGapBase_mm);
    const QString radius = number(r.core.majorRadius_mm);
    const QString lvRadial = number(r.winding.lvRadial_mm);
    const QString hvRadial = number(r.winding.hvRadial_mm);
    const QString mainDuct = number(r.winding.mainDuct_mm);
    const QString p12 = number(r.core.majorAngle_deg);
    const QString s12 = number(r.core.minorAngle_deg);
    const QString geometryNote = QStringLiteral("当前椭圆形算法的操作数展开；数值结果由引擎ROUND到两位小数，说明不额外取整。P12、S12分别读取本次原单角度快照，不按历史成员名称另行推导。单位mm，只读，不参与筛选。");
    text("AF33", QStringLiteral("π×(%1/2+%2+(%3+%4)/2)×%5/180=")
        .arg(diameter, phaseGap, lvRadial, number(lvDuctHeightSum), p12),
        geometryNote + QStringLiteral(" 对应AK33；末项为五级低压端部侧油道之和。原表文字先将半径取一位小数，数值公式没有该步骤；这里服从数值公式。"));
    text("AF34", QStringLiteral("π×(%1+%2+(%3+%4)/2)×%5/180=")
        .arg(radius, phaseGap, lvRadial, number(lvDuctWidthSum), s12),
        geometryNote + QStringLiteral(" 对应AK34；末项为五级低压相间侧油道之和。"));
    text("W36", QStringLiteral("π×(%1/2+%2+%3+%4+%5+(%6+%7)/2)×%8/180=")
        .arg(diameter, phaseGap, lvRadial, number(lvDuctHeightSum), mainDuct,
            hvRadial, number(hvDuctHeightSum), p12),
        geometryNote + QStringLiteral(" 对应AB36；低压、高压端部侧均累计五级；不使用原表文字的半径额外舍入。"));
    const QString longSideNote = geometryNote + QStringLiteral(" 对应AB39；实际数值公式仅累计前三级低压和高压相间侧油道。原W39拼接文字写五级，与数值公式不一致；本说明按现有数值算法显示，未改成五级。更改后两级仍可能经其他尺寸联动影响本式，不应理解为结果必然不变。");
    text("W39", QStringLiteral("π×(%1+%2+%3+%4+%5+(%6+%7)/2)×%8/180=")
        .arg(radius, phaseGap, lvRadial, number(lvFirstThreeWidths), mainDuct,
            hvRadial, number(hvFirstThreeWidths), s12), longSideNote);
    r.calculationSheetNotes.insert(QStringLiteral("AB39"), longSideNote);
    text("AF36", QStringLiteral("π×(%1/2+%2+%3+%4+%5/2)×%6/180=")
        .arg(diameter, phaseGap, lvRadial, number(lvDuctHeightSum), mainDuct, p12),
        geometryNote + QStringLiteral(" 对应AK36；五级低压端部侧油道累加，主空道只取半值；不使用原表文字的半径额外舍入。"));
    text("AF39", QStringLiteral("π×(%1+%2+%3+%4+%5/2)×%6/180=")
        .arg(radius, phaseGap, lvRadial, number(lvDuctWidthSum), mainDuct, s12),
        geometryNote + QStringLiteral(" 对应AK39；五级低压相间侧油道累加，主空道只取半值。"));

    for (int i = 0; i < 5; ++i) {
        const QString low = QStringLiteral("E%1").arg(29 + i);
        const QString high = QStringLiteral("E%1").arg(34 + i);
        const char *lvWidthWeight = i < 3 ? "AK34" : "AK33";
        const char *lvHeightWeight = i < 3 ? "AK33" : "AK34";
        const char *hvWidthWeight = i < 3 ? "AB39" : "AB36";
        const char *hvHeightWeight = i < 3 ? "AB36" : "AB39";
        const QString lvFormula = QStringLiteral("DE2%1=(%2×%3+%4×%5)/(%3+%5)=")
            .arg(i + 1).arg(number(in.lvDuctWidthSide[i]), detail(lvWidthWeight),
                number(in.lvDuctHeightSide[i]), detail(lvHeightWeight));
        const QString hvFormula = QStringLiteral("DE1%1=(%2×%3+%4×%5)/(%3+%5)=")
            .arg(i + 1).arg(number(in.hvDuctWidthSide[i]), detail(hvWidthWeight),
                number(in.hvDuctHeightSide[i]), detail(hvHeightWeight));
        const QString note = i < 3
            ? QStringLiteral("按原计算单第1—3级：相间侧×平均长＋端部侧×平均宽，再除以两者之和，结果取两位小数。")
            : QStringLiteral("已按原计算单第4、5级修正：相间侧×平均宽＋端部侧×平均长，再除以两者之和，结果取两位小数。非对称油道可改变漏磁与阻抗结果；旧寻优排序需重新寻优，不作整机合格判定。");
        text(qPrintable(low), lvFormula, note);
        text(qPrintable(high), hvFormula, note);
        r.calculationSheetNotes.insert(QStringLiteral("M%1").arg(29+i), note);
        r.calculationSheetNotes.insert(QStringLiteral("M%1").arg(34+i), note);
    }
    QStringList lambdaTerms = {number(r.winding.lvRadial_mm)};
    for (int i = 0; i < 5; ++i) lambdaTerms.append(detail(qPrintable(QStringLiteral("M%1").arg(29+i))));
    lambdaTerms.append(number(r.winding.mainDuct_mm));
    lambdaTerms.append(number(r.winding.hvRadial_mm));
    for (int i = 0; i < 5; ++i) lambdaTerms.append(detail(qPrintable(QStringLiteral("M%1").arg(34+i))));
    text("E39", QStringLiteral("λ=%1=").arg(lambdaTerms.join(QLatin1Char('+'))),
        QStringLiteral("当前数值公式累加全部五级低压和高压油道。原表E39拼接文字仅列前三项，与M39数值公式不一致；此处按实际五级计算说明。"));
    QStringList a2Terms = {number(r.winding.lvRadial_mm) + QStringLiteral("/3")};
    QStringList a1Terms = {number(r.winding.hvRadial_mm) + QStringLiteral("/3")};
    const char *segments[] = {"AF10", "AG10", "AH10", "AI10", "AJ10"};
    double cumulativeTurns = 0.0;
    int cumulativeLayers = 0;
    for (int i = 0; i < 5; ++i) {
        // 停用段的空白在现有数值链中按0累计；不把其他未接入字段当成0。
        cumulativeTurns += r.calculationSheetDetails.value(QString::fromLatin1(segments[i])).first;
        cumulativeLayers += r.winding.ductLayerIdx[i];
        a2Terms.append(QStringLiteral("%1×(%2/%3)²")
            .arg(detail(qPrintable(QStringLiteral("M%1").arg(29+i))), number(cumulativeTurns), number(in.lvTurns)));
        a1Terms.append(QStringLiteral("%1×[(%2−%3×%4)/%2]²")
            .arg(detail(qPrintable(QStringLiteral("M%1").arg(34+i))), number(r.winding.hvTurnsRated),
                number(cumulativeLayers), number(r.winding.layerCount)));
    }
    text("E40", QStringLiteral("a2=%1=").arg(a2Terms.join(QLatin1Char('+'))),
        QStringLiteral("五级M29:M33依次乘(AF10、AF10+AG10、…、AF10+…+AJ10)/AH8的平方，累加后取两位小数；停用段按0。"));
    text("E41", QStringLiteral("a1=%1=").arg(a1Terms.join(QLatin1Char('+'))),
        QStringLiteral("五级M34:M38依次乘[(Y8−累计Z30:Z34×Y9)/Y8]²；累计层序采用本次快照，结果取两位小数。"));
    text("E42", QStringLiteral("Sx=%1×%2+%3×%4+%5×%6=")
        .arg(number(r.impedance.a2), number(r.winding.lvMeanTurn_m*1000), number(r.winding.mainDuct_mm),
            detail("Q29"), number(r.impedance.a1), number(r.winding.hvMeanTurn_m*1000)));
    text("E43", QStringLiteral("X%=%1×3.95×%2×%3×%4/%5/%6/10^5=")
        .arg(number(r.impedance.kx), number(r.electrical.lvPhaseCurrent_A), number(in.lvTurns),
            number(r.impedance.leakArea_mm2), number(r.core.turnVoltage_V), number(r.impedance.hx_mm)));
    text("H44", QStringLiteral("Sg=0.75×%1×%2/10^6=").arg(number(r.mass.tankLength_mm), number(r.mass.tankWidth_mm)));
    const double effectiveWaves = r.oilExpansion.shortSideCount > 0.0
        ? r.oilExpansion.longSideCount+r.oilExpansion.shortSideCount-2.0 : r.oilExpansion.longSideCount-1.0;
    text("E45", QStringLiteral("Sbz=2×%1×(%2−6−2×%3+2×%4)×%5×%6/10^6=")
        .arg(number(effectiveWaves), number(in.wavePitch_mm), detail("S44"), number(in.waveDepth_mm),
            number(in.waveHeight_mm), detail("S50")),
        QStringLiteral("短边数>0：有效波纹数=S48+S49−2；否则=S48−1。各输入来自当前方案，结果取两位小数。"));
    text("E46", QStringLiteral("Sbj=%1×%2×%3/10^6=")
        .arg(r.oilExpansion.shortSideCount > 0.0 ? 8 : 4).arg(in.waveDepth_mm).arg(in.waveHeight_mm));
    text("E47", QStringLiteral("Sb=[(%1−2×45×%2)×%3+2×45×%2×(%3−%4)]/10^6=")
        .arg(number(2*(r.mass.tankLength_mm+r.mass.tankWidth_mm)), number(effectiveWaves),
            number(r.mass.tankHeight_mm), number(in.waveHeight_mm)));
    text("E49", QStringLiteral("QY=(%1+%2)/%3=")
        .arg(number(r.winding.loadLoss_W), number(r.core.noLoadLoss_W), number(r.thermal.totalSurface_m2)));
    text("J49", QStringLiteral("TY=0.262×%1^0.8=").arg(detail("I49")));
    text("H51", QStringLiteral("TM=1.2×%1+").arg(number(r.thermal.oilRise_K)));
    text("E50", QStringLiteral("Hf/Hs（参考）=(%1+%2+%3/2)/(%4+%5/2)=")
        .arg(number(in.tankBottomOil_mm), r.core.widths_mm.isEmpty() ? QStringLiteral("未接入") : number(r.core.widths_mm.first()),
            number(r.mass.windowHeight_mm), detail("Q26"), number(in.waveHeight_mm)),
        QStringLiteral("J23箱底油空＋G17首级片宽＋J14窗高/2，除以Q26箱顶油空＋S23波纹高/2；N50取两位小数。仅当前椭圆形/波纹油箱只读参考，不参与温升、合格判定或寻优筛选。"));
    text("U48", QStringLiteral("q1=(%1+%2)/%3=")
        .arg(number(r.winding.hvCopperLoss_W), number(r.winding.hvExtraLoss_W), number(r.thermal.hvSurface_m2)),
        QStringLiteral("按实际引擎显示高压电阻损耗+附加损耗。圆线附加项的实际值同样读取本次结果，不沿用原表文字中的省略分支。"));
    text("AD48", QStringLiteral("q2=(%1+%2)/%3=")
        .arg(number(r.winding.lvCopperLoss_W), number(r.winding.lvExtraLoss_W), number(r.thermal.lvSurface_m2)),
        QStringLiteral("当前低压热负荷包含低压附加损耗；原表拼接文字只写电阻损耗，这里与实际AK48计算口径一致。"));
    text("U49", QStringLiteral("τ1=0.065×%1^0.8=").arg(number(r.thermal.hvHeatLoad)));
    text("AD49", QStringLiteral("τ02=0.065×%1^0.8=").arg(number(r.thermal.lvHeatLoad)));
    const bool largeGap = r.thermal.hvLayerGap_mm > 0.64;
    int coolingCount = (r.thermal.mainDuctSecond_mm > 0.0 ? 1 : 0) + 1;
    for (int i = 0; i < 3; ++i) if (in.hvDuctHeightSide[i] > 0.0) coolingCount += 2;
    text("U50", largeGap ? QStringLiteral("τδ=0.002×(%1−0.64)").arg(number(r.thermal.hvLayerGap_mm)) : QString(),
        QStringLiteral("层间间隙≤0.64时原表说明留空，数值修正AC51按0参与；不代表字段遗漏。"));
    text("U51", largeGap ? QStringLiteral("×(%1−%2)×%3=").arg(in.hvTurnsPerLayer).arg(coolingCount)
        .arg(number(r.thermal.hvHeatLoad)) : QString(),
        QStringLiteral("散热计数=主空道第二油道标记+前三组高压端部油道各2+1；原表条件留空时不残留上一分支说明。"));
    text("U52", QStringLiteral("τc=0.002×%1×(%2−2×%3)×%4=")
        .arg(number(std::min(r.thermal.hvLayerGap_mm, 0.64))).arg(in.hvTurnsPerLayer).arg(coolingCount)
        .arg(number(r.thermal.hvHeatLoad)),
        QStringLiteral("按实际AC52公式：计数只含前三组端部油道，不照抄原U52字符串中的五组。修正项允许为负；汇总Y53时只叠加非负层间修正。"));
    int lvCoolingCount = 1;
    for (int i = 0; i < 5; ++i) if (in.lvDuctHeightSide[i] > 0.0) lvCoolingCount += 2;
    text("AD50", QStringLiteral("τc2=0.002×%1×(%2−2×%3)×%4=")
        .arg(number(in.lvLayerInsulCount*in.lvLayerInsul_mm)).arg(in.lvTurns).arg(lvCoolingCount)
        .arg(number(r.thermal.lvHeatLoad)),
        QStringLiteral("当前低压层间修正AK50；五组启用端部油道各计2再加1，结果取一位小数；允许负值，汇总AK51时只叠加非负层间修正。"));
    text("P49", r.thermal.oilRise_K > 42.0 ? QStringLiteral("需核对") : QString(),
        QStringLiteral("原表N49>42时显示问号，此处改为需核对。42是原表固定油面温升参考，不是用户油顶层温升限值；不改变寻优筛选。"));
    text("AA53", in.hvDuctHeightSide[1] != 0.0 && r.thermal.hvRiseAboveOil_K <= 20.0
        ? QStringLiteral("设一对半油道（参考）") : QString(),
        QStringLiteral("原表提示条件：Y31非0且Y53≤20。只作工艺参考，不自动改变油道配置，不新增合格判定。"));
    text("E52", QStringLiteral("按20℃注油，最高油平均温度70℃考核膨缩能力（参考）"));
    text("E53", QStringLiteral("最大油膨缩量："));
    text("E54", QStringLiteral("波纹膨胀量："));
    text("O53", QStringLiteral("kg"));
    text("O54", QStringLiteral("kg"));
    const auto &oil = r.oilExpansion;
    if (oil.available) {
        text("I53", QStringLiteral("%1×%2×%3=")
            .arg(number(oil.oilWeight_kg), number(oil.expansionCoefficient), number(oil.referenceDeltaT_K)));
        text("H54", QStringLiteral("0.06×%1×%2×%3×0.9×%4=")
            .arg(number(oil.waveHeight_mm/100), number(oil.waveDepth_mm/100),
                number(2*(oil.longSideCount+oil.shortSideCount)), number(oil.kp)));
        text("P54", oil.passed ? QStringLiteral("合格（膨缩参考）") : QStringLiteral("不合格（膨缩参考）"),
            QStringLiteral("仅膨缩参考：N54>N53才通过，相等不通过；不是整个方案的合格结论，也不参与寻优剔除。"));
    } else {
        for (const char *a : {"I53", "H54", "P54"}) text(a, QStringLiteral("不可用"), oil.error);
    }
}
} // namespace CalculationSheetDescriptions

#endif
