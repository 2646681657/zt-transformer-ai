#ifndef GRIDSEARCHSPACE_H
#define GRIDSEARCHSPACE_H

#include "IOptimizer.h"
#include <array>

// 连续量使用本次最细网格的整数坐标；整数/表内规格以一档为单位。
// 同一个坐标只对应一个输入，两阶段沿用同一坐标系和原始边界。
class GridSearchSpace {
public:
    using Point = std::array<int, 10>;
    GridSearchSpace(const CalcInput &base, const OptimizationSettings &settings)
        : m_base(base), m_settings(settings), m_scale(1 << settings.fineRoundCount()),
          m_ranges{settings.diameterRadius(), settings.straightRadius(), settings.lvTurnsRadius(),
                   settings.hvLayersRadius(), settings.lvFoilThickRadius(), settings.lvFoilWidthRadius(),
                   settings.hvBareWidthRadius(), settings.hvBareThickRadius()} {}

    Point coarsePoint(int index) const {
        Point point{};
        const int gradeCount = m_settings.searchSteelGrade ? int(m_settings.steelGrades.size()) : 1;
        point[9] = index % gradeCount;
        index /= gradeCount;
        const int count = m_settings.searchHvRoundWire ? int(m_settings.hvRoundWireDiameters.size()) : 1;
        point[8] = index % count;
        index /= count;
        for (int d = 7; d >= 0; --d) {
            const int size = 2 * m_ranges[d] + 1;
            point[d] = (index % size - m_ranges[d]) * (continuous(d) ? m_scale : 1);
            index /= size;
        }
        return point;
    }

    CalcInput inputFor(const Point &point) const {
        CalcInput in = m_base;
        // 不参与的变量不做浮点运算（也不读取其可能失效的历史步长）。
        if (m_ranges[0]) in.coreDiameter_mm += double(point[0]) / m_scale * m_settings.diaStep_mm;
        if (m_ranges[1]) in.coreStraight_mm += double(point[1]) / m_scale * m_settings.straightStep_mm;
        in.lvTurns += point[2];
        in.hvTurnsPerLayer += point[3];
        if (m_ranges[4]) in.lvFoilThick_mm += double(point[4]) / m_scale * m_settings.lvFoilThickStep_mm;
        if (m_ranges[5]) in.lvFoilWidth_mm += double(point[5]) / m_scale * m_settings.lvFoilWidthStep_mm;
        if (m_ranges[6]) in.hvBareWidth_mm += double(point[6]) / m_scale * m_settings.hvBareWidthStep_mm;
        if (m_ranges[7]) in.hvBareThick_mm += double(point[7]) / m_scale * m_settings.hvBareThickStep_mm;
        if (m_settings.searchHvRoundWire) {
            in.hvBareWidth_mm = m_settings.hvRoundWireDiameters[point[8]];
            in.hvBareThick_mm = in.hvBareWidth_mm;
        }
        if (m_settings.searchSteelGrade) {
            in.steelGrade = m_settings.steelGrades[point[9]];
            in.steelThickness_mm = CalcInput::thicknessFromSteelGrade(in.steelGrade);
        }
        // 本轮模式明确覆盖基准候选可能携带的旧价格快照。
        in.useCustomSteelPrice = m_settings.steelPricing == OptimizationSettings::CustomSteelPrices;
        in.steelPriceGrade = in.steelGrade.trimmed();
        in.steelPricePerKg = in.useCustomSteelPrice
            ? m_settings.steelGradePrices.value(in.steelGrade.trimmed().toUpper(), 0.0) : 17.0;
        in.materialPrices = m_settings.materialPrices.effective();
        return in;
    }

    QString steelGradeFor(const Point &point) const {
        return m_settings.searchSteelGrade ? m_settings.steelGrades[point[9]] : m_base.steelGrade.trimmed();
    }

    // 使用与搜索相同的整数坐标，不比较显示舍入值或浮点近似值。
    // 只报告非零范围，分类牌号无数值边界；圆线端点只针对冻结参与规格。
    int boundarySearchDimensions() const {
        int count = 0;
        for (int d = 0; d < 8; ++d)
            if (m_ranges[d] > 0 && !(d >= 6 && m_settings.searchHvRoundWire)) ++count;
        if (m_settings.searchHvRoundWire && m_settings.hvRoundWireDiameters.size() > 1) ++count;
        return count;
    }

    QStringList boundaryHits(const Point &point) const {
        QStringList hits;
        const auto in = inputFor(point);
        const QStringList names{QStringLiteral("铁芯直径"), QStringLiteral("直线段长"),
            QStringLiteral("低压匝数"), QStringLiteral("高压总层数W12"),
            QStringLiteral("低压箔厚"), QStringLiteral("低压箔宽"),
            QStringLiteral("高压裸线宽"), QStringLiteral("高压裸线厚")};
        const double values[]{in.coreDiameter_mm, in.coreStraight_mm, double(in.lvTurns), double(in.hvTurnsPerLayer),
            in.lvFoilThick_mm, in.lvFoilWidth_mm, in.hvBareWidth_mm, in.hvBareThick_mm};
        for (int d = 0; d < 8; ++d) {
            if (m_ranges[d] <= 0 || (d >= 6 && m_settings.searchHvRoundWire)) continue;
            const int radius = m_ranges[d] * (continuous(d) ? m_scale : 1);
            if (point[d] != -radius && point[d] != radius) continue;
            hits.append(QStringLiteral("%1：达到本轮%2，值%3%4。")
                .arg(names[d], point[d] == -radius ? QStringLiteral("下限") : QStringLiteral("上限"))
                .arg(values[d], 0, 'g', 15).arg(d == 2 || d == 3 ? QString() : QStringLiteral(" mm")));
        }
        const int count = int(m_settings.hvRoundWireDiameters.size());
        if (m_settings.searchHvRoundWire && count > 1 && (point[8] == 0 || point[8] == count - 1))
            hits.append(QStringLiteral("高压圆线规格：达到本轮参与清单%1端点，直径%2 mm；仅指所选档范围，不代表完整线规表端点。")
                .arg(point[8] == 0 ? QStringLiteral("下") : QStringLiteral("上"))
                .arg(in.hvBareWidth_mm, 0, 'g', 15));
        return hits;
    }

    QVector<Point> neighbors(const Point &center, int round) const {
        QVector<Point> points(1, center);
        // 牌号是分类变量：细搜保持中心牌号，不按名单顺序取“相邻牌号”。
        // 粗搜已遍历每个勾选牌号；其他9维及原有全局最低成本3中心规则不变。
        for (int d = 0; d < 9; ++d) {
            const int stride = continuous(d) ? m_scale >> round : 1;
            const int radius = d == 8 ? 0 : m_ranges[d] * (continuous(d) ? m_scale : 1);
            const int lower = d == 8 ? 0 : -radius;
            const int upper = d == 8 ? (m_settings.searchHvRoundWire
                ? int(m_settings.hvRoundWireDiameters.size()) - 1 : 0) : radius;
            QVector<Point> expanded;
            for (const auto &point : points) {
                // 不移动到裁剪后的非格点：只有中心及±stride且在边界内的点。
                for (int offset = -1; offset <= 1; ++offset) {
                    const int value = center[d] + offset * stride;
                    if (value < lower || value > upper) continue;
                    Point next = point;
                    next[d] = value;
                    expanded.append(next);
                }
            }
            points = std::move(expanded);
        }
        return points;
    }

    static QString key(const Point &point) {
        QStringList coordinates;
        for (int value : point) coordinates.append(QString::number(value));
        return coordinates.join(QLatin1Char(','));
    }
private:
    static bool continuous(int d) { return d != 2 && d != 3 && d != 8; }
    const CalcInput &m_base;
    const OptimizationSettings &m_settings;
    int m_scale;
    std::array<int, 8> m_ranges;
};

#endif // GRIDSEARCHSPACE_H
