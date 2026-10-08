#ifndef SEARCHCOVERAGEPLANNER_H
#define SEARCHCOVERAGEPLANNER_H

#include "SearchCenterPool.h"
#include <functional>

// Strategy only. No physics, prices or relaxed constraints; one fixed lattice for the run.
class SearchCoveragePlanner {
public:
    using Point = GridSearchSpace::Point;
    struct Region {
        Point center{};
        std::array<QVector<int>, 11> axes;
        std::array<int, 11> cursor{};
        bool exhausted = false;
        bool next(Point &point) {
            if (exhausted) return false;
            point = center;
            for (int d = 0; d < 11; ++d) point[d] = axes[d][cursor[d]];
            for (int d = 10; d >= 0; --d) {
                if (++cursor[d] < axes[d].size()) return true;
                cursor[d] = 0;
            }
            exhausted = true;
            return true;
        }
    };
    SearchCoveragePlanner(const GridSearchSpace &space, const OptimizationSettings &settings)
        : m_space(space), m_settings(settings), m_initialLow(space.initialLower()),
          m_initialHigh(space.initialUpper()), m_hardLow(m_initialLow), m_hardHigh(m_initialHigh) {
        if (!settings.enhancedCoverage || !settings.expandCoverage) return;
        const auto ranges = settings.numericRanges();
        const auto steps = settings.numericSteps();
        for (int n = 0; n < 9; ++n) {
            if (!ranges[n] || settings.hardMinimumTexts[n].trimmed().isEmpty()) continue;
            const int d = GridSearchSpace::pointDimension(n);
            const double base = space.numericValue(n, 0);
            const double unit = steps[n] / space.coarseStride(d);
            const double lower = settings.hardMinimumTexts[n].trimmed().toDouble();
            const double upper = settings.hardMaximumTexts[n].trimmed().toDouble();
            m_hardLow[d] = int(std::ceil((lower - base) / unit));
            m_hardHigh[d] = int(std::floor((upper - base) / unit));
            // Recover valid endpoints lost to division rounding, still checking the physical value.
            while (space.numericValue(n, m_hardLow[d] - 1) >= lower) --m_hardLow[d];
            while (space.numericValue(n, m_hardHigh[d] + 1) <= upper) ++m_hardHigh[d];
            while (space.numericValue(n, m_hardLow[d]) < lower) ++m_hardLow[d];
            while (space.numericValue(n, m_hardHigh[d]) > upper) --m_hardHigh[d];
        }
    }
    void recordProcessed(const Point &point, const CalcInput &input) {
        const auto values = OptimizationSettings::numericBases(input);
        if (!m_haveActual) { m_actualLow = m_actualHigh = values; m_haveActual = true; }
        else for (int n = 0; n < 9; ++n) {
            m_actualLow[n] = qMin(m_actualLow[n], values[n]);
            m_actualHigh[n] = qMax(m_actualHigh[n], values[n]);
        }
        auto it = m_gradeBounds.find(point[9]);
        if (it == m_gradeBounds.end())
            it = m_gradeBounds.insert(point[9], {m_initialLow, m_initialHigh});
        for (int d = 0; d < 11; ++d) {
            it.value().low[d] = qMin(it.value().low[d], point[d]);
            it.value().high[d] = qMax(it.value().high[d], point[d]);
        }
    }
    QVector<Region> localRegions(const QVector<SearchCenterPool::Center> &centers, int round) const {
        QVector<Region> regions;
        for (const auto &center : centers) {
            const Bounds bounds = boundsFor(center.point[9]);
            regions.append(neighborhood(center.point, round, bounds.low, bounds.high));
        }
        return regions;
    }
    QVector<Region> expansionRegions(const QVector<SearchCenterPool::Center> &centers, int phase) const {
        QVector<Region> regions;
        if (!m_settings.enhancedCoverage || !m_settings.expandCoverage) return regions;
        const auto ranges = m_settings.numericRanges();
        // One outside face per touched direction, at most one original coarse step.
        // Dimensions/directions rotate between phases; centers are interleaved below.
        for (int i = 0; i < 18; ++i) {
            const int direction = (i + phase) % 18;
            const int n = direction / 2, sign = direction % 2 ? 1 : -1;
            if (!ranges[n] || m_settings.hardMinimumTexts[n].trimmed().isEmpty()) continue;
            const int d = GridSearchSpace::pointDimension(n);
            for (const auto &center : centers) {
                const Bounds bounds = boundsFor(center.point[9]);
                const int edge = sign < 0 ? bounds.low[d] : bounds.high[d];
                if (center.point[d] != edge) continue;
                const int face = sign < 0 ? qMax(m_hardLow[d], edge - m_space.coarseStride(d))
                    : qMin(m_hardHigh[d], edge + m_space.coarseStride(d));
                if ((sign < 0 && face >= edge) || (sign > 0 && face <= edge)) continue;
                Region region = neighborhood(center.point, 1, bounds.low, bounds.high);
                region.axes[d] = QVector<int>{face};
                regions.append(std::move(region));
            }
        }
        return regions;
    }
    // Lazy Cartesian generators interleave one point per region. The consumer can pause,
    // stop, deduplicate or bound scheduling to remaining budget without huge products.
    static bool visitInterleaved(QVector<Region> regions, const std::function<bool(const Point &)> &visit) {
        bool havePoint = true;
        while (havePoint) {
            havePoint = false;
            for (auto &region : regions) {
                Point point;
                if (!region.next(point)) continue;
                havePoint = true;
                if (!visit(point)) return false;
            }
        }
        return true;
    }
    QStringList coverageDetails() const {
        QStringList lines;
        const auto names = OptimizationSettings::numericNames();
        const auto ranges = m_settings.numericRanges();
        for (int n = 0; n < 9; ++n) {
            const int d = GridSearchSpace::pointDimension(n);
            const bool roundWire = m_settings.searchHvRoundWire && (n == 6 || n == 7);
            const QString initial = roundWire
                ? interval(m_settings.hvRoundWireDiameters.first(), m_settings.hvRoundWireDiameters.last())
                : interval(m_space.numericValue(n, m_initialLow[d]), m_space.numericValue(n, m_initialHigh[d]));
            const QString explored = m_haveActual
                ? interval(m_actualLow[n], m_actualHigh[n])
                : QStringLiteral("无已处理点");
            QString hard;
            const bool supplied = !m_settings.hardMinimumTexts[n].trimmed().isEmpty()
                || !m_settings.hardMaximumTexts[n].trimmed().isEmpty();
            if (supplied)
                hard = QStringLiteral("[%1, %2]%3")
                    .arg(m_settings.hardMinimumTexts[n], m_settings.hardMaximumTexts[n],
                         m_settings.enhancedCoverage && m_settings.expandCoverage
                         ? QString() : QStringLiteral("（未启用，保留文本）"));
            else hard = QStringLiteral("未填写，不扩展");
            lines.append(QStringLiteral("%1%2：初始%3；实际处理探索包围盒%4；硬边界%5；%6。")
                .arg(names[n], n == 2 || n == 3 ? QString() : QStringLiteral(" mm"), initial, explored, hard,
                    roundWire ? QStringLiteral("由冻结圆线清单驱动，此数值维硬边界不用于扩展线规")
                    : ranges[n] > 0 ? QStringLiteral("参与网格") : QStringLiteral("固定/零范围，不扩展")));
        }
        lines.append(QStringLiteral("实际探索仅统计唯一已处理点（含线型及工艺预检）；各维极值包围盒不代表内部已穷举，不构成全局最优证明。"));
        lines.append(QStringLiteral("牌号及圆线清单冻结，不自动新增；性能、材料价格和工艺条件冻结，低改善只结束当前局部细化。"));
        return lines;
    }
private:
    struct Bounds { Point low, high; };
    Bounds boundsFor(int grade) const { return m_gradeBounds.value(grade, {m_initialLow, m_initialHigh}); }
    Region neighborhood(const Point &center, int round, const Point &low, const Point &high) const {
        Region region;
        region.center = center;
        for (int d = 0; d < 11; ++d) {
            if (d == 9) { region.axes[d].append(center[d]); continue; }
            const int stride = m_space.fineStride(d, round);
            // Center first ensures fair region coverage under a partial phase budget.
            for (int offset : {0, -1, 1}) {
                const int value = center[d] + offset * stride;
                if (value >= low[d] && value <= high[d]
                        && value >= m_hardLow[d] && value <= m_hardHigh[d]) region.axes[d].append(value);
            }
            if (region.axes[d].isEmpty()) region.exhausted = true;
        }
        return region;
    }
    static QString interval(double a, double b) {
        return QStringLiteral("[%1, %2]").arg(a, 0, 'g', 15).arg(b, 0, 'g', 15);
    }
    const GridSearchSpace &m_space;
    const OptimizationSettings &m_settings;
    Point m_initialLow, m_initialHigh, m_hardLow, m_hardHigh;
    std::array<double, 9> m_actualLow{}, m_actualHigh{};
    bool m_haveActual = false;
    QMap<int, Bounds> m_gradeBounds;
};

#endif // SEARCHCOVERAGEPLANNER_H
