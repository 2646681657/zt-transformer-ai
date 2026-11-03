#ifndef TESTVOLTAGEHINTS_H
#define TESTVOLTAGEHINTS_H

#include <QString>
#include <cmath>

// 原计算单 C7/C8 的参考提示，不用于绝缘合格判定或任何计算公式。
struct TestVoltageHints {
    QString highVoltage = QStringLiteral("需人工核对");
    QString lowVoltage = QStringLiteral("需人工核对");
    QString highReason = QStringLiteral("尚未生成计算单参考");
    QString lowReason = QStringLiteral("尚未生成计算单参考");

    static QString sourceNote()
    {
        return QStringLiteral("计算单C7/C8参考；非绝缘合格判定");
    }
};

inline TestVoltageHints testVoltageHints(double hvRated_kV, double lvRated_kV)
{
    TestVoltageHints hints;
    if (!std::isfinite(hvRated_kV) || hvRated_kV <= 0.0) {
        hints.highReason = QStringLiteral("高压额定电压须为有效正数");
    } else if (hvRated_kV > 40.5) {
        hints.highReason = QStringLiteral("高压超过计算单40.5kV范围");
    } else {
        hints.highVoltage = hvRated_kV <= 1.0 ? QStringLiteral("AC5")
            : hvRated_kV <= 3.6 ? QStringLiteral("LI40AC18")
            : hvRated_kV <= 7.2 ? QStringLiteral("LI60AC25")
            : hvRated_kV <= 12.0 ? QStringLiteral("LI75AC35")
            : hvRated_kV <= 24.0 ? QStringLiteral("LI125AC55")
            : QStringLiteral("LI200AC85");
        hints.highReason = QStringLiteral("计算单C7，依据已提交高压额定电压");
    }
    if (!std::isfinite(lvRated_kV) || lvRated_kV <= 0.0) {
        hints.lowReason = QStringLiteral("低压额定电压须为有效正数");
    } else if (lvRated_kV >= 1.0) {
        hints.lowReason = QStringLiteral("低压≥1kV，计算单C8未提供参考值");
    } else {
        hints.lowVoltage = QStringLiteral("AC5");
        hints.lowReason = QStringLiteral("计算单C8，依据已提交低压额定电压");
    }
    return hints;
}

#endif
