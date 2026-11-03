#ifndef STEELGRADESEARCH_H
#define STEELGRADESEARCH_H

#include "IOptimizer.h"
#include "DesignDatabase.h"
#include <QSet>

// 不修改数据库或引擎查表规则；只接受唯一牌号及完整有限的现有性能曲线。
inline QString steelGradeSearchError(const DesignDatabase &db, const QString &name, QString *canonical = nullptr)
{
    const QString requested = name.trimmed();
    const SteelCurve *selected = nullptr;
    for (const auto &curve : db.steelCurves()) {
        if (curve.grade.trimmed().compare(requested, Qt::CaseInsensitive) != 0) continue;
        if (selected) return QStringLiteral("牌号%1在数据库中重复，不能用于联合寻优。").arg(requested);
        selected = &curve;
    }
    if (!selected) return QStringLiteral("牌号%1未收录，请重新勾选有效牌号。").arg(requested);
    const QString grade = selected->grade.trimmed();
    double minT = 0.0, maxT = 0.0;
    if (CalcInput::thicknessFromSteelGrade(grade) <= 0.0 || selected->points.size() < 2
            || !db.steelCurveRange(grade, minT, maxT))
        return QStringLiteral("牌号%1片厚格式或磁密曲线无效，不能用于联合寻优。").arg(grade);
    for (const auto &point : selected->points) {
        // 加载器缺失数值会落为0；正磁密点要求正铁损/磁化容量，避免把缺数据当零损耗材料。
        if (!std::isfinite(point.wPerKg) || point.wPerKg <= 0.0
                || !std::isfinite(point.vaPerKg) || point.vaPerKg <= 0.0)
            return QStringLiteral("牌号%1缺少有效铁损/磁化容量数据，不能用于联合寻优。").arg(grade);
    }
    if (canonical) *canonical = grade;
    return {};
}

// GUI线程准备。失败时清空快照并保留原因，不能静默删去失效的用户选择。
inline void prepareSteelGradeSearch(OptimizationSettings &settings)
{
    settings.steelGrades.clear();
    settings.steelGradeSelectionError.clear();
    if (!settings.searchSteelGrade) return;
    auto &db = DesignDatabase::instance();
    if (!db.isLoaded() && !db.load()) {
        settings.steelGradeSelectionError = QStringLiteral("硅钢曲线加载失败：") + db.lastError();
        return;
    }
    if (settings.selectedSteelGrades.isEmpty()) {
        settings.steelGradeSelectionError = QStringLiteral("请显式勾选至少一个候选牌号，不会自动补入当前牌号。");
        return;
    }
    QSet<QString> seen;
    for (const auto &name : settings.selectedSteelGrades) {
        QString canonical;
        QString error = steelGradeSearchError(db, name, &canonical);
        if (error.isEmpty() && seen.contains(canonical.toUpper()))
            error = QStringLiteral("候选牌号%1重复，请重新选择。").arg(canonical);
        if (!error.isEmpty()) {
            settings.steelGrades.clear();
            settings.steelGradeSelectionError = error;
            return;
        }
        seen.insert(canonical.toUpper());
        settings.steelGrades.append(canonical);
    }
}

#endif
