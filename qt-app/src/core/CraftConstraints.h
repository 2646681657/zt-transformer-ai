#ifndef CRAFTCONSTRAINTS_H
#define CRAFTCONSTRAINTS_H

#include <QString>
#include <cmath>

// User supplied manufacturing evidence, independent of performance limits.
struct CraftConstraints {
    bool enabled = false;
    double minimumMainDuct_mm = 0.0;
    QString source;

    QString validationError() const {
        if (!enabled) return {};
        if (!std::isfinite(minimumMainDuct_mm) || minimumMainDuct_mm <= 0.0)
            return QStringLiteral("企业主油道最小宽度须为有限正数。");
        if (source.trimmed().isEmpty())
            return QStringLiteral("启用企业主油道约束须填写企业认可资料的来源备注。");
        return {};
    }
    QString rejectionReason(double width) const {
        const QString error = validationError();
        if (!error.isEmpty()) return error;
        if (!enabled) return {};
        if (!std::isfinite(width) || width < minimumMainDuct_mm)
            return QStringLiteral("主油道宽度%1 mm低于企业最小值%2 mm（来源：%3）。")
                .arg(width, 0, 'g', 15).arg(minimumMainDuct_mm, 0, 'g', 15).arg(source.trimmed());
        return {};
    }
    QString description() const {
        if (!enabled) return QStringLiteral("企业工艺未校核：未启用主油道最小宽度；其他制造限值及库存规格待企业资料核对。");
        const QString error = validationError();
        if (!error.isEmpty()) return QStringLiteral("企业工艺配置无效：") + error;
        return QStringLiteral("企业主油道最小宽度%1 mm，来源：%2；仅校核此项，其他制造限值及库存规格待核对。")
            .arg(minimumMainDuct_mm, 0, 'g', 15).arg(source.trimmed());
    }
};

#endif // CRAFTCONSTRAINTS_H
