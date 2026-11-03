#ifndef CORELAMINATIONPREVIEW_H
#define CORELAMINATIONPREVIEW_H

#include "DesignDatabase.h"
#include "StructureConfig.h"
#include <cmath>

// Independent geometry only. Never feeds CalcInput, CalcResult or optimization.
namespace CoreLaminationPreview {
struct Stage {
    double width_mm = 0.0;
    double stack_mm = 0.0;       // Original half-stack, not Sheet1 actual lamination thickness.
    double arcCumulative_mm = 0.0; // Excludes the separately displayed central rectangle.
};
struct Result {
    bool available = false;
    QString error;
    double matchedReference_mm = 0.0;
    double centralWidth_mm = 0.0;
    double centralFullThickness_mm = 0.0;
    QVector<Stage> stages;
};

inline Result calculate(StructureConfig::CoreShape shape, double diameter, double straight,
                        const QVector<CoreLaminationRow> &rows, const QVector<double> &widthOffsets = {})
{
    const auto failure = [](const QString &message) {
        Result unavailable;
        unavailable.error = message;
        return unavailable;
    };
    if(!widthOffsets.isEmpty()){
        if(widthOffsets.size()!=20)return failure(QStringLiteral("片宽修正必须包含20级参数。"));
        for(double value:widthOffsets)if(!std::isfinite(value))return failure(QStringLiteral("片宽修正包含无效数值。"));
    }
    if (shape != StructureConfig::Circle && shape != StructureConfig::LongRound)
        return failure(QStringLiteral("本区仅预览圆形/长圆形几何；椭圆沿用上方已有预览，其他形状未接入。"));
    if (!std::isfinite(diameter) || diameter <= 0.0)
        return failure(QStringLiteral("铁芯直径H12须为大于零的有效数值。"));
    if (shape == StructureConfig::LongRound && (!std::isfinite(straight) || straight < 0.0))
        return failure(QStringLiteral("长圆形直线段K12须为非负有效数值。"));
    if (rows.isEmpty()) return failure(QStringLiteral("项目叠积表无数据，不能预览。"));
    const CoreLaminationRow *matched = nullptr;
    double previous = 0.0;
    for (const auto &row : rows) {
        if (!std::isfinite(row.ref) || row.ref <= previous)
            return failure(QStringLiteral("项目叠积表参考尺寸非正或非严格升序，不能预览。"));
        previous = row.ref;
        if (row.ref <= diameter) matched = &row;
    }
    // Preview refuses extrapolation even though Excel LOOKUP can saturate above its last row.
    if (!matched || diameter > rows.last().ref)
        return failure(QStringLiteral("直径H12超出项目叠积表范围[%1, %2]mm；不取端点代替、不外推。")
            .arg(rows.first().ref).arg(rows.last().ref));
    if (matched->widths.size() != 11 || matched->yokeBase.size() != 4)
        return failure(QStringLiteral("命中叠积行缺少11级片宽或4列补充片基数，不能预览。"));

    Result result;
    result.matchedReference_mm = matched->ref;
    if (shape == StructureConfig::LongRound) {
        result.centralWidth_mm = diameter;    // 线圈型式!B20=计算单!F17=H12
        result.centralFullThickness_mm = straight; // D20=计算单!F18=K12
    }
    static const double offsets[11] = {0, 5, 15, 25, 45, 60, 90, 130, 120, 110, 100};
    static const double supplementOffsets[4] = {90, 80, 60, 0};
    const double radiusSquared = (diameter / 2.0) * (diameter / 2.0);
    if (!std::isfinite(radiusSquared)) return failure(QStringLiteral("直径平方超出有效数值范围。"));
    double cumulative = 0.0;
    for (int i = 0; i < 15; ++i) {
        const double base = i < 11 ? matched->widths[i] : matched->yokeBase[i - 11];
        if (!std::isfinite(base) || base < 0.0)
            return failure(QStringLiteral("第%1级片宽基数无效，不能预览。原单偏移须加在有效基数上。").arg(i + 1));
        const double width = base + (widthOffsets.isEmpty() ? (i < 11 ? offsets[i] : supplementOffsets[i - 11]) : widthOffsets[i]);
        if(!std::isfinite(width)||width<0)return failure(QStringLiteral("第%1级修正后片宽为负或无效。").arg(i+1));
        double stack = 0.0;
        // G18:L18 have no zero-width guard. M18:S18 and F20:I20 do.
        if (!(i >= 6 && width == 0.0)) {
            const double radicand = radiusSquared - (width / 2.0) * (width / 2.0);
            if (!std::isfinite(radicand) || radicand < 0.0)
                return failure(QStringLiteral("第%1级片宽%2mm放不进直径%3mm：根式无解，不钳位或借用椭圆结果。")
                    .arg(i + 1).arg(width).arg(diameter));
            // Non-ellipse G18:S18 and F20:I20: subtract prior rounded half-stacks.
            stack = std::round(std::sqrt(radicand) - cumulative);
            if (!std::isfinite(stack) || stack < 0.0)
                return failure(QStringLiteral("第%1级几何叠厚为负或无效，请核对片宽表与直径；不排序、不取绝对值。").arg(i + 1));
        }
        cumulative += stack;
        if (!std::isfinite(cumulative)) return failure(QStringLiteral("圆弧累计叠厚超出有效范围。"));
        result.stages.append({width, stack, cumulative});
    }
    result.available = true;
    return result;
}
} // namespace CoreLaminationPreview

#endif
