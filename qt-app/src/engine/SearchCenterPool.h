#ifndef SEARCHCENTERPOOL_H
#define SEARCHCENTERPOOL_H

#include "GridSearchSpace.h"
#include <algorithm>
#include <limits>

class SearchCenterPool {
public:
    struct Center { GridSearchSpace::Point point; double cost; qint64 order; };
    static constexpr int perGradeCapacity = 256;
    explicit SearchCenterPool(const GridSearchSpace &space) : m_space(space) {
        GridSearchSpace::Point point{};
        for (int grade = 0; grade <= space.initialUpper()[9]; ++grade) {
            point[9] = grade;
            m_pools.insert(space.steelGradeFor(point), {});
        }
    }
    void add(const GridSearchSpace::Point &point, double cost, qint64 order) {
        auto &pool = m_pools[m_space.steelGradeFor(point)];
        for (const auto &c : pool) if (c.point == point) return;
        Center c{point, cost, order};
        pool.insert(std::lower_bound(pool.begin(), pool.end(), c, better), c);
        if (pool.size() > perGradeCapacity) { pool.removeLast(); ++m_evicted[m_space.steelGradeFor(point)]; }
    }
    QVector<Center> select(int limit, int phase) const {
        QVector<Center> result;
        if (m_pools.isEmpty()) return result;
        Center best{};
        bool haveBest = false;
        for (const auto &pool : m_pools) if (!pool.isEmpty() && (!haveBest || better(pool.first(), best))) {
            best = pool.first(); haveBest = true;
        }
        if (!haveBest) return result;
        result.append(best);
        const auto grades = m_pools.keys();
        for (int i = 0; i < grades.size() && result.size() < limit; ++i) {
            const auto &pool = m_pools[grades[(phase + i) % grades.size()]];
            if (!pool.isEmpty() && pool.first().point != best.point) result.append(pool.first());
        }
        while (result.size() < limit) {
            double farthest = -1.0;
            Center chosen{};
            bool found = false;
            for (const auto &pool : m_pools) for (const auto &candidate : pool) {
                bool exists = false;
                double nearest = std::numeric_limits<double>::infinity();
                for (const auto &selected : result) {
                    if (candidate.point == selected.point) { exists = true; break; }
                    if (candidate.point[9] == selected.point[9])
                        nearest = qMin(nearest, distanceSquared(candidate.point, selected.point));
                }
                if (exists) continue;
                if (!found || nearest > farthest || (nearest == farthest && better(candidate, chosen))) {
                    found = true; farthest = nearest; chosen = candidate;
                }
            }
            if (!found) break;
            result.append(chosen);
        }
        return result;
    }
    QStringList details(const QString &label, const QVector<Center> &centers, bool selection = true) const {
        QMap<QString, int> represented;
        for (const auto &c : centers) ++represented[m_space.steelGradeFor(c.point)];
        QStringList lines;
        lines.append(selection
            ? QStringLiteral("%1：选择%2个中心；有可行中心时全局最低成本中心必选，每牌号池上限256，独立于展示池。")
                .arg(label).arg(centers.size())
            : QStringLiteral("%1：每牌号中心池上限256，以下为最终保留/淘汰统计，非新增中心阶段。").arg(label));
        for (auto it = m_pools.cbegin(); it != m_pools.cend(); ++it) {
            if (!selection) {
                lines.append(QStringLiteral("%1 · %2：池保留%3/256，累计淘汰%4；仅最终池统计，不表明已细搜。")
                    .arg(label, it.key()).arg(it.value().size()).arg(m_evicted.value(it.key())));
            } else {
                lines.append(QStringLiteral("%1 · %2：中心%3，池保留%4/256，累计淘汰%5；%6。")
                    .arg(label, it.key()).arg(represented.value(it.key())).arg(it.value().size())
                    .arg(m_evicted.value(it.key())).arg(it.value().isEmpty()
                        ? QStringLiteral("没有可行中心，本阶段未获中心")
                        : represented.value(it.key()) > 0 ? QStringLiteral("本阶段获得中心")
                        : QStringLiteral("本阶段未获得中心，后续阶段轮转；不代表已细搜")));
            }
        }
        lines.append(QStringLiteral("池裁剪会丢弃较高成本区域；中心和探索包围盒不能证明全体区域覆盖或全局最优。"));
        return lines;
    }
private:
    static bool better(const Center &a, const Center &b) {
        return a.cost < b.cost || (a.cost == b.cost && a.order < b.order);
    }
    double distanceSquared(const GridSearchSpace::Point &a, const GridSearchSpace::Point &b) const {
        double distance = 0.0;
        for (int d = 0; d < 11; ++d) {
            if (d == 9) continue;
            const double delta = (double(a[d]) - b[d]) / m_space.coarseStride(d);
            distance += delta * delta;
        }
        return distance;
    }
    const GridSearchSpace &m_space;
    QMap<QString, QVector<Center>> m_pools;
    QMap<QString, int> m_evicted;
};

#endif // SEARCHCENTERPOOL_H
