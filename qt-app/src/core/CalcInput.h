#ifndef CALCINPUT_H
#define CALCINPUT_H
// 电磁计算输入（设计变量）：与 SB20 计算单的手工输入项一一对应
// 默认值即 SB20-M-630-10 计算单的取值，可作为寻优变量空间

#include <QString>
#include <QRegularExpression>
#include <QVector>
#include <cmath>
#include "PerformanceCriteria.h"

struct CalcInput {
    // 持久化/校核元数据，不参与电磁实际值公式。
    bool hasPerformanceCriteria = false;
    TransformerParams performanceCriteria;
    // 线圈型式!J17：牌号前两位代表硅钢片厚度的百分之一毫米。
    static double thicknessFromSteelGrade(const QString &grade)
    {
        static const QRegularExpression pattern(QStringLiteral("^[0-9]{2}[A-Za-z0-9]+$"));
        const QString value = grade.trimmed();
        if (!pattern.match(value).hasMatch()) {
            return 0.0;
        }
        return value.left(2).toInt() / 100.0;
    }

    // ---- 额定值 ----
    double capacity_kVA = 630.0;      // H1 容量
    double hvRated_kV = 10.0;         // J1 高压
    double lvRated_kV = 0.4;          // L1 低压
    int hvTapPlusSteps = 2;           // 正向调压级数
    int hvTapMinusSteps = 2;          // 负向调压级数（存正数，显示为 -2）
    double hvTapStep_pct = 2.5;       // 每级调压电压百分比
    double hvTapMax_pct = 5.0;        // 分接上限（±2×2.5% → +5%）
    double hvTapMin_pct = -5.0;       // 分接下限
    bool hvDeltaConnected = true;     // 高压 D 接（false 为 Y 接）
    bool lvStarConnected = true;      // 低压 yn 接（false 为 d 接）

    // ---- 铁芯（椭圆形）----
    double coreDiameter_mm = 170.0;   // H12 直径（长轴方向直线段的直径）
    double coreStraight_mm = 103.0;   // K12 直线长
    double ellipseAngle_deg = 18.0;   // M12 椭圆角
    double stackFactor = 0.95;        // L16 叠片系数
    double steelThickness_mm = 0.18;  // 硅钢片厚（由牌号前缀决定）
    QString steelGrade = QStringLiteral("18SQGD065");  // 硅钢牌号
    int seamCount = 5;                // 接缝数
    double coreLossCraftCoef = 1.23;  // 心柱工艺系数（Sheet1 T25），保留旧字段名
    double yokeLossCraftCoef = 1.23;  // 铁轭工艺系数（Sheet1 W25）
    // 宽90支持F20自动几何；手填值独立保留，宽80/60仍是工艺手动值。
    bool yokePiece1Auto = true;
    double yokePiece1Stack_mm = 4.0;  // 宽 90 片叠厚（Sheet1 D16）
    double yokePiece2Stack_mm = 1.5;  // 宽 80 片叠厚（Sheet1 D17 手输）
    double yokePiece3Stack_mm = 2.0;  // 宽 60 片叠厚（Sheet1 D18 手输）
    // 轭片宽工艺放大（Sheet1 C6..C8 手输 180：小于该宽度的前若干级片宽抬高）
    double yokeWidenTo_mm = 180.0;    // 抬到的片宽（0=不放大）
    int yokeWidenStages = 3;          // 抬高的级数（C6..C8 共 3 级）
    // 轭阶梯工艺值（Sheet1 E 列 16 级，轭各级外伸半宽；-1 表示按 (C5-Ci)/2 计算）
    double yokeSteps_mm[16] = {0, -1, -1, -1, 0, 0, 5, 10, 15, 25, 35, -1, -1, -1, 0, 0};

    // ---- 高压绕组 ----
    double hvBareWidth_mm = 2.05;     // X13 裸线宽（扁线）
    double hvBareThick_mm = 5.52;     // Z13 裸线厚
    int hvParallelCount = 1;          // AB13 并绕
    int hvStackCount = 1;             // AB14 叠绕
    // 保持计算单及旧方案的宽=厚圆线判定；规格由数据库在计算入口校验。
    bool isRoundHighVoltageWire() const { return hvBareWidth_mm == hvBareThick_mm; }
    QString highVoltageWireError() const
    {
        if (!std::isfinite(hvBareWidth_mm) || !std::isfinite(hvBareThick_mm) ||
            hvBareWidth_mm <= 0.0 || hvBareThick_mm <= 0.0)
            return QStringLiteral("高压裸线宽、厚必须是大于零的有效数值");
        if (hvParallelCount <= 0 || hvStackCount <= 0)
            return QStringLiteral("高压并绕、叠绕根数必须是大于零的整数");
        if (isRoundHighVoltageWire()) return QString(); // 圆线不使用扁线绝缘增厚输入
        if (!std::isfinite(hvWireInsulAdd_mm) || hvWireInsulAdd_mm < 0.0)
            return QStringLiteral("高压导线绝缘增厚必须是大于等于零的有效数值");
        return QString();
    }
    int hvTurnsPerLayer = 15;         // W12 高压总层数；保留旧字段名以兼容方案文件
    double hvLayerInsul_mm = 0.0967;  // X35 层间绝缘厚
    double hvWireInsulAdd_mm = 0.15;  // X14/Z14 的总尺寸增厚（不是单边厚度）
    QString hvWireInsulation = QStringLiteral("QZB-2/130"); // U14；Custom保留手填值
    static double insulationIncrement(const QString &type)
    {
        if (type == QLatin1String("QZB-2/130")) return 0.15;
        if (type == QLatin1String("ZB-0.3") || type == QLatin1String("ZLB-0.3")) return 0.35;
        if (type == QLatin1String("ZB-0.45") || type == QLatin1String("ZLB-0.45")) return 0.50;
        return -1.0;
    }
    // 数值仍是计算引擎的唯一来源；旧代码/导入数据有不一致时不能丢弃手填值。
    QString resolvedInsulationType() const
    {
        if (isRoundHighVoltageWire()) return QStringLiteral("RoundTable");
        const double preset = insulationIncrement(hvWireInsulation);
        return preset >= 0.0 && std::abs(preset - hvWireInsulAdd_mm) < 1e-9
            ? hvWireInsulation : QStringLiteral("Custom");
    }
    bool hvCopperWire = true;         // 高压铜导线（false 铝导线）
    int hvCoilFormIdx = 1;            // AC9：1=圆筒式，2=分段圆筒式（两段串联）；2仍禁算
    // 高压轴向油道（宽侧 X30..X34 / 高侧 Y30..Y34，0=无）
    double hvDuctWidthSide[5] = {4, 4, 4, 4, 4};
    double hvDuctHeightSide[5] = {4, 4, 4, 4, 4};

    // ---- 低压绕组（铜箔或铝箔）----
    int lvTurns = 18;                 // AH8 低压匝数
    double lvFoilThick_mm = 1.35;     // AF14 箔厚
    double lvFoilWidth_mm = 323.0;    // AJ14 箔宽
    int lvLayerInsulCount = 1;        // AG26 层间绝缘层数
    double lvLayerInsul_mm = 0.18;    // AI26 层间绝缘厚
    double lvEndInsul_mm = 17.5;      // AI25 端绝缘
    bool lvCopperFoil = true;         // 低压铜箔（false 铝箔）
    // 低压轴向油道（宽侧 AG28..AG32 / 高侧 AH28..AH32，0=无）
    double lvDuctWidthSide[5] = {3.5, 3.5, 3.5, 3.5, 3.5};
    double lvDuctHeightSide[5] = {3.5, 3.5, 3.5, 3.5, 3.5};

    QString oilDuctInputError() const
    {
        for (int side = 0; side < 2; ++side) {
            const double *widths = side == 0 ? hvDuctWidthSide : lvDuctWidthSide;
            const double *heights = side == 0 ? hvDuctHeightSide : lvDuctHeightSide;
            const QString winding = side == 0 ? QStringLiteral("高压") : QStringLiteral("低压");
            for (int i = 0; i < 5; ++i) {
                if (!std::isfinite(widths[i]) || widths[i] < 0.0)
                    return QStringLiteral("%1油道宽 %2（相间侧）必须是大于等于零的有效数值，0表示该侧停用").arg(winding).arg(i + 1);
                if (!std::isfinite(heights[i]) || heights[i] < 0.0)
                    return QStringLiteral("%1油道高 %2（端部侧）必须是大于等于零的有效数值，0表示该侧停用").arg(winding).arg(i + 1);
            }
        }
        return QString();
    }

    // 原表独立判定两侧的0值；这些布局只提示，不擅自配对、重排或禁算。
    QString oilDuctLayoutNote() const
    {
        QString note;
        for (int side = 0; side < 2; ++side) {
            const double *widths = side == 0 ? hvDuctWidthSide : lvDuctWidthSide;
            const double *heights = side == 0 ? hvDuctHeightSide : lvDuctHeightSide;
            const QString winding = side == 0 ? QStringLiteral("高压") : QStringLiteral("低压");
            bool stopped = false;
            for (int i = 0; i < 5; ++i) {
                if ((widths[i] > 0.0) != (heights[i] > 0.0))
                    note += QStringLiteral("%1油道%2仅单侧启用；").arg(winding).arg(i + 1);
                if (heights[i] == 0.0) stopped = true;
                else if (stopped)
                    note += QStringLiteral("%1端部油道%2在停用油道之后仍启用；").arg(winding).arg(i + 1);
            }
        }
        return note.isEmpty() ? QString() : note + QStringLiteral("分段与散热需人工核对，程序未自动重排或禁算");
    }

    // ---- 主空道 ----
    double mainDuctWidth_mm = 3.5;    // AD43 高低压间油道
    double mainDuctInsul_mm = 1.0;    // AF43 纸板厚

    // ---- 损耗系数 ----
    double strayLossFactor = 0.11;    // J10 杂散损耗系数
    double leadLoss_W = 0.0;          // 引线损耗记录值，未参与计算；原表J10是杂散系数，非此字段
    double lvExtraLoss_W = 0.0;       // AS45 低压附加损耗

    // ---- 油箱（波纹油箱）----
    double tankBottomOil_mm = 19.0;   // J23 箱底油空
    double tankSideClear_mm = 88.0;   // F24 器身侧净空
    double tankEndClear_mm = 62.0;    // H25 器身端净空
    double tankFoot_mm = 323.0;       // J26 垫脚高
    int waveDepth_mm = 150;           // S45 波纹深
    int waveHeight_mm = 900;          // S46 波纹高
    int wavePitch_mm = 41;            // S47 波纹节距

    // ---- 其他结构常数 ----
    double phaseGapBase_mm = 3.0;     // B28 相间距基础
    double refFluxDens_T = 1.7;       // AM8 低压参考磁密
};

#endif  // CALCINPUT_H
