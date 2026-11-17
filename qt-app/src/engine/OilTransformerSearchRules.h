#ifndef OILTRANSFORMERSEARCHRULES_H
#define OILTRANSFORMERSEARCHRULES_H

#include "CalcInput.h"
#include "CalcResult.h"
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace OilTransformerSearchRules {

inline double stackFactorForGrade(const CalcInput &input)
{
    const double thickness = CalcInput::thicknessFromSteelGrade(input.steelGrade);
    if (thickness == 0.18 || thickness == 0.20) return 0.95;
    if (thickness == 0.23) return 0.96;
    if (thickness == 0.27) return 0.97;
    return input.stackFactor;
}

inline bool inRange(double value, double minimum, double maximum)
{
    return value >= minimum && value <= maximum;
}

inline QStringList inputFailures(const CalcInput &input)
{
    QStringList failures;
    if (!inRange(input.coreDiameter_mm, 70, 300))
        failures << QStringLiteral("企业铁芯直径 70–300 mm");
    if (!inRange(input.ellipseAngle_deg, 10, 30))
        failures << QStringLiteral("企业椭圆角 10–30°");
    if (input.lvCopperFoil && !inRange(input.lvFoilWidth_mm, 150, 1200))
        failures << QStringLiteral("企业低压铜箔宽度 150–1200 mm");
    if (!inRange(input.lvFoilThick_mm, 0.35, 2.2))
        failures << QStringLiteral("企业低压单箔厚度 0.35–2.2 mm");
    if (input.lvLayerInsulCount != 1 || std::abs(input.lvLayerInsul_mm - 0.18) > 1e-9)
        failures << QStringLiteral("企业低压单箔层间绝缘 1 张 × 0.18 mm");
    if (input.isRoundHighVoltageWire()) {
        if (input.hvBareWidth_mm > 2.65)
            failures << QStringLiteral("企业高压圆线直径 ≤2.65 mm");
    } else {
        // The workbook's X13/Z13 names describe winding directions. The
        // conductor specification uses the smaller side as its thickness.
        const double thickness = std::min(input.hvBareWidth_mm, input.hvBareThick_mm);
        const double width = std::max(input.hvBareWidth_mm, input.hvBareThick_mm);
        if (!inRange(thickness, 1.2, 5.6))
            failures << QStringLiteral("企业高压扁线厚度 1.2–5.6 mm");
        if (!inRange(width, 3, 16))
            failures << QStringLiteral("企业高压扁线宽度 3–16 mm");
        if (thickness > 0 && !inRange(width / thickness, 1.3, 5))
            failures << QStringLiteral("企业高压扁线宽厚比 1.3–5");
    }
    if (input.hvParallelCount < 1 || input.hvParallelCount > 3 || input.hvStackCount != 1)
        failures << QStringLiteral("企业高压导线 1–3 并、1 叠");
    if (!inRange(input.waveDepth_mm, 50, 400))
        failures << QStringLiteral("企业波纹片深度 50–400 mm");
    if (!inRange(input.waveHeight_mm, 300, 1400))
        failures << QStringLiteral("企业波纹片高度 300–1400 mm");
    return failures;
}

inline QStringList resultFailures(const CalcInput &input, const CalcResult &result)
{
    QStringList failures;
    const double majorAxis = input.coreDiameter_mm + input.coreStraight_mm;
    if (result.core.minorAxis_mm <= 0 || !inRange(majorAxis / result.core.minorAxis_mm, 1.2, 1.85))
        failures << QStringLiteral("企业铁芯长短轴比 1.2–1.85");
    if (!(result.core.fluxDensity_core_T < 1.75))
        failures << QStringLiteral("企业心柱设计磁密 <1.75 T");
    if (!(result.winding.lvCurrentDensity <= (input.lvCopperFoil ? 4.5 : 2.2)))
        failures << QStringLiteral("企业低压箔材电密（铜 ≤4.5，铝 ≤2.2 A/mm²）");
    if (!(result.winding.hvCurrentDensity <= (input.hvCopperWire ? 4.5 : 2.2)))
        failures << QStringLiteral("企业高压导线电密（铜 ≤4.5，铝 ≤2.2 A/mm²）");
    if (input.hvCoilFormIdx == 2 && input.hvRated_kV == 20
            && !inRange(result.winding.hvSegmentGap_mm, 30, 35))
        failures << QStringLiteral("企业 20 kV 段间距 30–35 mm");
    if (input.hvCoilFormIdx == 2 && input.hvRated_kV == 35
            && !inRange(result.winding.hvSegmentGap_mm, 40, 45))
        failures << QStringLiteral("企业 35 kV 段间距 40–45 mm");
    return failures;
}

} // namespace OilTransformerSearchRules

#endif
