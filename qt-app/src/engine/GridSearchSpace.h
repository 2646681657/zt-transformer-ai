#ifndef GRIDSEARCHSPACE_H
#define GRIDSEARCHSPACE_H

#include "IOptimizer.h"
#include <array>

// 连续量坐标以原步长的一半为单位；整数/表内规格以一档为单位。
// 同一个坐标只对应一个输入，两阶段沿用同一坐标系和原始边界。
class GridSearchSpace {
public:
    using Point = std::array<int, 9>;
    GridSearchSpace(const CalcInput &base, const OptimizationSettings &settings)
        : m_base(base), m_settings(settings),
          m_ranges{settings.diameterRadius(), settings.straightRadius(), settings.lvTurnsRadius(),
                   settings.hvLayersRadius(), settings.lvFoilThickRadius(), settings.lvFoilWidthRadius(),
                   settings.hvBareWidthRadius(), settings.hvBareThickRadius()} {}

    Point coarsePoint(int index) const {
        Point point{};
        const int count = m_settings.searchHvRoundWire ? int(m_settings.hvRoundWireDiameters.size()) : 1;
        point[8] = index % count;
        index /= count;
        for (int d = 7; d >= 0; --d) {
            const int size = 2 * m_ranges[d] + 1;
            point[d] = (index % size - m_ranges[d]) * (continuous(d) ? 2 : 1);
            index /= size;
        }
        return point;
    }

    CalcInput inputFor(const Point &point) const {
        CalcInput in = m_base;
        // 不参与的变量不做浮点运算（也不读取其可能失效的历史步长）。
        if (m_ranges[0]) in.coreDiameter_mm += point[0] * 0.5 * m_settings.diaStep_mm;
        if (m_ranges[1]) in.coreStraight_mm += point[1] * 0.5 * m_settings.straightStep_mm;
        in.lvTurns += point[2];
        in.hvTurnsPerLayer += point[3];
        if (m_ranges[4]) in.lvFoilThick_mm += point[4] * 0.5 * m_settings.lvFoilThickStep_mm;
        if (m_ranges[5]) in.lvFoilWidth_mm += point[5] * 0.5 * m_settings.lvFoilWidthStep_mm;
        if (m_ranges[6]) in.hvBareWidth_mm += point[6] * 0.5 * m_settings.hvBareWidthStep_mm;
        if (m_ranges[7]) in.hvBareThick_mm += point[7] * 0.5 * m_settings.hvBareThickStep_mm;
        if (m_settings.searchHvRoundWire) {
            in.hvBareWidth_mm = m_settings.hvRoundWireDiameters[point[8]];
            in.hvBareThick_mm = in.hvBareWidth_mm;
        }
        return in;
    }

    QVector<Point> neighbors(const Point &center) const {
        QVector<Point> points(1, center);
        for (int d = 0; d < 9; ++d) {
            const int radius = d == 8 ? 0 : m_ranges[d] * (continuous(d) ? 2 : 1);
            const int lower = d == 8 ? 0 : -radius;
            const int upper = d == 8 ? (m_settings.searchHvRoundWire
                ? int(m_settings.hvRoundWireDiameters.size()) - 1 : 0) : radius;
            QVector<Point> expanded;
            for (const auto &point : points) {
                for (int value = qMax(lower, center[d] - 1); value <= qMin(upper, center[d] + 1); ++value) {
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
    std::array<int, 8> m_ranges;
};

#endif // GRIDSEARCHSPACE_H
