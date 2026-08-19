#include "ElectromagneticEngine.h"

#include <QtMath>
#include <QCoreApplication>
#include <QDebug>

#include "DesignDatabase.h"

// ============================================================================
// 工具函数：Excel 语义复现
// ============================================================================
namespace {

// Excel ROUND(x, n)：四舍五入（half away from zero）
double excelRound(double x, int n)
{
    const double f = std::pow(10.0, n);
    return std::round(x * f) / f;
}

// Excel INT(x)：向下取整
int excelInt(double x)
{
    return static_cast<int>(std::floor(x));
}

// Excel CEILING(x, sig)：向上取整到 sig 的倍数（sig>0）
double excelCeiling(double x, double sig)
{
    if (sig <= 0.0) {
        return x;
    }
    return std::ceil(x / sig - 1e-9) * sig;
}

// 全链路计算上下文：承载计算单各单元格中间量（注释标注单元格位置）
struct EmCtx {
    const CalcInput *in = nullptr;
    CalcResult *out = nullptr;
    bool failed = false;
    QString error;

    void fail(const QString &msg)
    {
        if (!failed) {
            failed = true;
            error = msg;
        }
    }

    // ---- 基础电量 ----
    double vLineMax_V = 0.0;      // V5 高压线电压（最高分接）
    double vLineRated_V = 0.0;    // Y5
    double vLineMin_V = 0.0;      // AA5
    double vPhaseMax_V = 0.0;     // V6
    double vPhaseRated_V = 0.0;   // Y6
    double vPhaseMin_V = 0.0;     // AA6
    double lvLine_V = 0.0;        // AH5
    double lvPhase_V = 0.0;       // AH6
    double hvLineCurrent_A = 0.0; // W7 高压线电流
    double hvPhaseCurrent_A = 0.0;// Z7 高压相电流
    double lvPhaseCurrent_A = 0.0;// AG7 低压相电流

    // ---- 铁芯椭圆几何 ----
    double majorR_mm = 0.0;       // M20 大圆半径
    double yokeFlat_mm = 0.0;     // O20 圆心到轴
    double junctionH_mm = 0.0;    // R20 交接点高
    double minorAxis_mm = 0.0;    // L20 短轴长
    double majorAngle_deg = 0.0;  // P12 大圆心角
    double minorAngle_deg = 0.0;  // S12 小圆心角
    double halfMinor_mm = 0.0;    // B27 = L20/2

    // ---- 叠积（16 级，Sheet1 行 5..20）----
    double lamWidth[16] = {0};    // B 列 片宽
    double yokeWidth[16] = {0};   // C 列 轭片宽
    double stack[16] = {0};       // D 列 叠厚
    double yokeStep[16] = {0};    // E 列 轭阶梯
    double coreArea_cm2 = 0.0;    // D23/F14 心柱截面
    double yokeArea_cm2 = 0.0;    // J23 铁轭截面
    double coreAreaAct_cm2 = 0.0; // I24 实际心柱截面
    double yokeAreaAct_cm2 = 0.0; // O24 实际铁轭截面

    // ---- 高压绕组布局 ----
    int hvTurnsMax = 0;           // V8
    int hvTurnsRated = 0;         // Y8
    int hvTurnsMin = 0;           // AA8
    int segLayers[3] = {0, 0, 0}; // W9/W10/W11 三段层数
    int segTurnsPerLayer = 0;     // Y9 每层匝数
    int ductLayerIdx[5] = {0};    // Z30..Z34 油道层序（缓存值语义）
    double x42Sheets = 0.0;       // X42 层间绝缘张数
    double x41Sheets = 0.0;       // X41
    double hvInsWidth_mm = 0.0;   // X14 绝缘线宽
    double hvInsThick_mm = 0.0;   // Z14 绝缘线厚
    double hvWireSection_mm2 = 0.0;// U15/AA15
    double hvAxialPerWire_mm = 0.0; // AA22 = Z14×AB13
    double hvLayerInsulTotal_mm = 0.0;  // X25 层间绝缘总厚
    double hvRadial_mm = 0.0;     // W28 高压辐向厚
    double hvAxial_mm = 0.0;      // AA28 高压轴向高
    double hvInnerAxial_mm = 0.0; // AC30 高压内孔轴向高
    double lvAxialOuter_mm = 0.0; // AA33 低压轴向（箔宽+端绝缘）

    // ---- 低压绕组布局 ----
    double lvRadial_mm = 0.0;     // AK23 低压辐向厚
    double lvWireSection_mm2 = 0.0; // AH15

    // ---- 主空道 / 几何链 ----
    double mainDuct_mm = 0.0;     // AK43
    double bChain_mm = 0.0;       // B45 半链长
    double bChainFull_mm = 0.0;   // B47 全链长
    double windowHeight_mm = 0.0; // J14 窗高
    double centerDist_mm = 0.0;   // B49 中心距

    // ---- 平均匝长几何 ----
    double ab36Wide_mm = 0.0;     // AB36 高压平均宽（宽侧）
    double ab39Long_mm = 0.0;     // AB39 高压平均长（长侧）
    double ak33Wide_mm = 0.0;     // AK33 低压平均宽
    double ak34Long_mm = 0.0;     // AK34 低压平均长
    double ak36Wide_mm = 0.0;     // AK36 主空道平均宽
    double ak39Long_mm = 0.0;     // AK39 主空道平均长
    double hvMeanTurn_m = 0.0;    // X17
    double lvMeanTurn_m = 0.0;    // AH17

    // ---- 油箱尺寸 ----
    double tankWidth_mm = 0.0;    // F25
    double tankLength_mm = 0.0;   // H26
    double tankHeight_mm = 0.0;   // J27
    double waveLongSides = 0.0;   // S48 波纹长边
    double waveShortSides = 0.0;  // S49 波纹短边（0=单侧）
    double waveThick_mm = 0.0;    // S44 波纹厚

    // ---- 油道等效宽（阻抗用） ----
    double m29_33[5] = {0};       // M29..M33 低压油道等效宽
    double m34_38[5] = {0};       // M34..M38 高压油道等效宽
    double lambda_mm = 0.0;       // M39 漏磁总厚
    double ac30_mm = 0.0;         // AC30
};

// ============================================================================
// 1. 基础电量（计算单行 5..7）
// ============================================================================
void calcElectrical(EmCtx &c)
{
    const CalcInput &in = *c.in;
    const double cap = in.capacity_kVA;

    // 高压线电压（V5/Y5/AA5）：额定 ± 分接
    c.vLineRated_V = in.hvRated_kV * 1000.0;
    c.vLineMax_V = c.vLineRated_V * (1.0 + in.hvTapMax_pct / 100.0);
    c.vLineMin_V = c.vLineRated_V * (1.0 + in.hvTapMin_pct / 100.0);

    // 高压相电压（V6/Y6/AA6）：D 接相=线；Y 接相=线/√3
    const double sqrt3 = std::sqrt(3.0);
    auto hvPhase = [sqrt3](double vLine, bool delta) {
        return excelRound(delta ? vLine : vLine / sqrt3, 0);
    };
    c.vPhaseMax_V = hvPhase(c.vLineMax_V, in.hvDeltaConnected);
    c.vPhaseRated_V = hvPhase(c.vLineRated_V, in.hvDeltaConnected);
    c.vPhaseMin_V = hvPhase(c.vLineMin_V, in.hvDeltaConnected);

    // 低压（AH5/AH6）：yn 接相=线/√3；d 接相=线
    c.lvLine_V = in.lvRated_kV * 1000.0;
    c.lvPhase_V = in.lvStarConnected
                      ? excelRound(c.lvLine_V / sqrt3, 0)
                      : c.lvLine_V;

    // 电流（W7/Z7/AG7）
    c.hvLineCurrent_A = excelRound(cap / sqrt3 / in.hvRated_kV, 2);
    c.hvPhaseCurrent_A = in.hvDeltaConnected
                             ? excelRound(c.hvLineCurrent_A / sqrt3, 2)
                             : c.hvLineCurrent_A;
    c.lvPhaseCurrent_A = in.lvStarConnected
                             ? excelRound(cap / sqrt3 / c.lvLine_V * 1000.0, 2)
                             : excelRound(cap / 3.0 / c.lvLine_V * 1000.0, 2);

    // 匝电压（AC4）：低压相电压 / 低压匝数
    c.out->core.turnVoltage_V = excelRound(c.lvPhase_V / in.lvTurns, 4);
}

// ============================================================================
// 2. 铁芯椭圆几何与叠积（计算单行 12/17/18/20 + Sheet1 行 5..20）
// ============================================================================
void calcCoreGeometry(EmCtx &c)
{
    const CalcInput &in = *c.in;
    const double D = in.coreDiameter_mm;          // H12
    const double Ls = in.coreStraight_mm;         // K12
    const double ang = in.ellipseAngle_deg;       // M12
    const double rad = ang * M_PI / 180.0;

    // 椭圆参数（L20/M20/O20/R20/P12/S12）
    c.majorR_mm = excelRound(D / 2.0 + Ls / 2.0 / std::sin(rad), 2);
    c.yokeFlat_mm = excelRound(Ls / 2.0 / std::tan(rad), 0);
    c.junctionH_mm = excelRound(c.majorR_mm * std::sin(rad), 0);
    c.minorAxis_mm = excelRound(2.0 * (c.majorR_mm - Ls / 2.0 / std::tan(rad)), 0);
    c.majorAngle_deg = 180.0 - 2.0 * ang;
    c.minorAngle_deg = 2.0 * ang;
    c.halfMinor_mm = c.minorAxis_mm / 2.0;

    // 叠积片宽（G17..S17）：叠积表阶梯查 + 列偏移
    DesignDatabase &db = DesignDatabase::instance();
    QVector<double> widths;
    if (!db.coreWidths(c.minorAxis_mm, widths) || widths.size() < 11) {
        c.fail(QStringLiteral("叠积表中未找到短轴长 %1 的片宽数据").arg(c.minorAxis_mm));
        return;
    }
    // 计算单 G17..S17 的列偏移（B..L 列，G17 无偏移）
    static const double kColOffset[11] = {0, 5, 15, 25, 45, 60, 90, 130, 120, 110, 100};
    // 轭片基数（F19..I19，M..P 列偏移 90/80/60/0）
    QVector<double> yokeBase;
    db.coreYokeBase(c.minorAxis_mm, yokeBase);
    const double yokeBaseOff[4] = {90, 80, 60, 0};

    for (int i = 0; i < 11; ++i) {
        c.lamWidth[i] = widths[i] + kColOffset[i];
    }
    for (int i = 0; i < 4 && i + 11 < 16; ++i) {
        const double base = (yokeBase.size() > i ? yokeBase[i] : 0.0) + yokeBaseOff[i];
        c.lamWidth[11 + i] = base;
    }
    c.lamWidth[15] = 0.0;

    // 叠厚递推（G18..S18）：小圆弧段用大圆半径，超出交接高切换小圆+直线段
    double acc = 0.0;
    for (int i = 0; i < 11; ++i) {
        const double halfW = c.lamWidth[i] / 2.0;
        const double t = std::sqrt(c.majorR_mm * c.majorR_mm
                                   - (halfW + c.yokeFlat_mm) * (halfW + c.yokeFlat_mm));
        double cand;
        if (t <= c.junctionH_mm) {
            cand = t;
        } else {
            cand = std::sqrt((D / 2.0) * (D / 2.0) - halfW * halfW) + Ls / 2.0;
        }
        const double d = excelRound(cand - acc, 0);
        c.stack[i] = d;
        acc += d;
    }
    // T 形轭补充片叠厚（F20/G20/H20 → Sheet1 D16..D18）
    c.stack[11] = in.yokePiece1Stack_mm;
    c.stack[12] = in.yokePiece2Stack_mm;
    c.stack[13] = in.yokePiece3Stack_mm;
    c.stack[14] = 0.0;
    c.stack[15] = 0.0;

    // 轭片宽（Sheet1 C 列）：C6..C8 工艺放大（<180 的前 yokeWidenStages 级抬到 yokeWidenTo）
    for (int i = 0; i < 16; ++i) {
        c.yokeWidth[i] = c.lamWidth[i];
    }
    if (in.yokeWidenTo_mm > 0.0 && in.yokeWidenStages > 0) {
        for (int i = 1; i <= in.yokeWidenStages && i < 16; ++i) {
            if (c.yokeWidth[i] < in.yokeWidenTo_mm) {
                c.yokeWidth[i] = in.yokeWidenTo_mm;
            }
        }
    }

    // 轭阶梯（Sheet1 E 列）：-1 表示按 (C5-Ci)/2
    for (int i = 0; i < 16; ++i) {
        const double v = in.yokeSteps_mm[i];
        c.yokeStep[i] = (v < 0.0)
                            ? (c.yokeWidth[0] - c.yokeWidth[i]) / 2.0
                            : v;
    }

    // 心柱/铁轭截面（D23/J23）：首级叠厚在 Sheet1 中计为 D5=2×G18，其余级×2 对称
    double sumCol = 0.0;
    double sumYoke = 0.0;
    for (int i = 0; i < 16; ++i) {
        sumCol += c.lamWidth[i] * c.stack[i] * 2.0;
        sumYoke += c.yokeWidth[i] * c.stack[i] * 2.0;
    }
    c.coreArea_cm2 = sumCol * in.stackFactor / 100.0;
    c.yokeArea_cm2 = sumYoke * in.stackFactor / 100.0;
    c.out->core.coreArea_cm2 = c.coreArea_cm2;
    c.out->core.yokeArea_cm2 = c.yokeArea_cm2;
    c.out->core.majorRadius_mm = c.majorR_mm;
    c.out->core.yokeFlat_mm = c.yokeFlat_mm;
    c.out->core.junctionHeight_mm = c.junctionH_mm;
    c.out->core.minorAxis_mm = c.minorAxis_mm;
    for (int i = 0; i < 16; ++i) {
        c.out->core.widths_mm.append(c.lamWidth[i]);
        c.out->core.stacks_mm.append(c.stack[i]);
    }

    // 设计磁密（F16 = 匝电压×450/截面/10）
    c.out->core.fluxDensity_core_T = excelRound(
        c.out->core.turnVoltage_V * 450.0 / c.coreArea_cm2 / 10.0, 3);
}

// ============================================================================
// 3. 绕组布局（匝数/层分布/辐向/轴向/平均匝长）
// ============================================================================
void calcWindingLayout(EmCtx &c)
{
    const CalcInput &in = *c.in;
    const double et = c.out->core.turnVoltage_V;  // AC4

    // 高压匝数（V8/Y8/AA8）
    c.hvTurnsMax = excelRound(c.vPhaseMax_V / et, 0);
    c.hvTurnsRated = excelRound(c.vPhaseRated_V / et, 0);
    c.hvTurnsMin = excelRound(c.vPhaseMin_V / et, 0);
    c.out->winding.hvTurnsMax = c.hvTurnsMax;
    c.out->winding.hvTurnsRated = c.hvTurnsRated;
    c.out->winding.hvTurnsMin = c.hvTurnsMin;
    c.out->winding.lvTurns = in.lvTurns;

    // 层分布（W9/W10/W11 段层数，Y9 每层匝数）
    const int w12 = in.hvTurnsPerLayer;                       // W12
    const bool hasDuct2 = in.hvDuctHeightSide[1] > 0.0;       // Y31
    c.segLayers[0] = (hasDuct2 ? excelInt(w12 / 3.0) : excelInt(w12 / 3.0) + 1) - 1;  // W9
    c.segTurnsPerLayer = excelInt(c.hvTurnsMax / double(w12)) + 1;                    // Y9
    c.segLayers[2] = 1;                                       // W11 手输 1
    c.segLayers[1] = w12 - c.segLayers[0] - c.segLayers[2];   // W10

    // 油道层序（Z30..Z34）：采用计算单缓存值语义（工艺分段），从输入读取
    const int ductIdx[5] = {1, 1, 2, 2, 1};
    for (int i = 0; i < 5; ++i) {
        c.ductLayerIdx[i] = ductIdx[i];
    }

    // 绝缘线尺寸（X14/Z14）：QZB 漆包 +0.15 / ZB-0.3 +0.35 / ZB-0.45 +0.5
    const double add = in.hvWireInsulAdd_mm;
    c.hvInsWidth_mm = in.hvBareWidth_mm + add;
    c.hvInsThick_mm = in.hvBareThick_mm + add;

    // 导线截面（U15）：扁线扣除圆角
    const double r = (in.hvBareWidth_mm < 1.7) ? 0.5
                     : (in.hvBareWidth_mm < 2.5) ? 0.65
                     : (in.hvBareWidth_mm < 4.0) ? 0.8 : 1.0;
    c.hvWireSection_mm2 = excelRound(
        in.hvBareWidth_mm * in.hvBareThick_mm - 0.8584 * r * r, 3);
    c.lvWireSection_mm2 = in.lvFoilThick_mm * in.lvFoilWidth_mm;   // AH15
    c.out->winding.hvWireSection_mm2 = c.hvWireSection_mm2;
    c.out->winding.lvWireSection_mm2 = c.lvWireSection_mm2;

    // 电密（X16/AH16）
    c.out->winding.hvCurrentDensity = excelRound(
        c.hvPhaseCurrent_A / (c.hvWireSection_mm2 * in.hvParallelCount), 3);
    c.out->winding.lvCurrentDensity = excelRound(
        c.lvPhaseCurrent_A / c.lvWireSection_mm2, 3);

    // 层间绝缘张数（X42/X41）与高压辐向（W24..W28）
    c.x42Sheets = excelCeiling(
        2.0 * c.segTurnsPerLayer * et
            / (in.hvCoilFormIdx * 4900.0 * in.hvLayerInsul_mm), 1.0);
    c.x41Sheets = (c.x42Sheets < 5.0)
                      ? c.x42Sheets
                      : (std::fmod(c.x42Sheets, 2.0) == 1.0
                             ? std::floor(c.x42Sheets / 2.0) + 1
                             : c.x42Sheets / 2.0);
    const double w24 = c.hvInsWidth_mm * w12;   // W24 = X14×W12
    double x25;                                 // X25 层间绝缘总厚
    if (w12 % 2 == 1) {
        x25 = ((w12 - 1) / 2.0 * (c.x41Sheets + c.x42Sheets) + 3.0) * in.hvLayerInsul_mm;
    } else {
        x25 = ((w12 / 2.0 - 1.0) * (c.x41Sheets + c.x42Sheets) + c.x42Sheets + 3.0)
              * in.hvLayerInsul_mm;
    }
    const double w26 = w24 + x25;               // W26
    c.hvLayerInsulTotal_mm = x25;
    c.hvRadial_mm = excelRound(w26 * 1.05 / 0.5, 0) * 0.5;   // W28

    // 低压辐向（AK23）：箔厚×匝数 + 层间绝缘，×1.05 工艺系数
    const double ak23raw = (in.lvFoilThick_mm * in.lvTurns
                            + (in.lvTurns - 1) * in.lvLayerInsulCount * in.lvLayerInsul_mm)
                           * 1.05;
    c.lvRadial_mm = excelRound(ak23raw / 0.5, 0) * 0.5;

    // 主空道（AK43 = AD43+AF43+等级加宽）
    const double kV = in.hvRated_kV;
    double ak43 = in.mainDuctWidth_mm + in.mainDuctInsul_mm;
    if (kV > 12.0) {
        ak43 += 5.0;
    }
    if (kV > 24.0) {
        ak43 += 1.5 + 5.0;
    }
    c.mainDuct_mm = ak43;

    // 轴向尺寸（AA24..AA28/AC30/AA33）
    c.hvAxialPerWire_mm = c.hvInsThick_mm * in.hvParallelCount;   // AA22
    const double aa24 = excelRound(
        c.hvAxialPerWire_mm * (c.segTurnsPerLayer + 1), 2);       // AA24
    const double aa25 = (in.hvStackCount > 1) ? c.hvInsThick_mm : 0.0;
    const double aa26 = aa24 + aa25;                              // AA26
    c.hvAxial_mm = excelRound(aa26 * 1.01 / 5.0, 0) * 5.0 + 1.0;  // AA28
    c.lvAxialOuter_mm = in.lvFoilWidth_mm + 2.0 * in.lvEndInsul_mm; // AA33/J14
    const double aa29 = (in.hvCoilFormIdx == 1) ? 0.0 : (kV > 24.0 ? 40.0 : 35.0);
    const double aa30 = c.hvAxial_mm + aa29;                      // AA30
    c.ac30_mm = excelRound(aa30 - c.hvAxialPerWire_mm, 2);        // AC30

    // 低压分匝段（AF10..AJ10）：各油道间匝数
    // 油道均设时按 INT(AH8/6) 分段（计算单语义）
    int lvDuctActive = 0;
    for (int i = 0; i < 5; ++i) {
        if (in.lvDuctHeightSide[i] > 0.0) {
            ++lvDuctActive;
        }
    }
    // 平均匝长几何（AB36/AB39/AK33/AK34/AK36/AK39）
    // P12/S12 为角度分数（144/180、36/180）：Excel 弧长 = π×半径×角度/180
    const double P12 = c.majorAngle_deg / 180.0;
    const double S12 = c.minorAngle_deg / 180.0;
    const double B28 = in.phaseGapBase_mm;
    const double B29 = c.halfMinor_mm + B28;   // B29
    const double sumLvDuctH = in.lvDuctHeightSide[0] + in.lvDuctHeightSide[1]
                              + in.lvDuctHeightSide[2] + in.lvDuctHeightSide[3]
                              + in.lvDuctHeightSide[4];
    const double sumLvDuctW = in.lvDuctWidthSide[0] + in.lvDuctWidthSide[1]
                              + in.lvDuctWidthSide[2] + in.lvDuctWidthSide[3]
                              + in.lvDuctWidthSide[4];
    const double sumHvDuctH = in.hvDuctHeightSide[0] + in.hvDuctHeightSide[1]
                              + in.hvDuctHeightSide[2] + in.hvDuctHeightSide[3]
                              + in.hvDuctHeightSide[4];
    const double sumHvDuctW = in.hvDuctWidthSide[0] + in.hvDuctWidthSide[1]
                              + in.hvDuctWidthSide[2] + in.hvDuctWidthSide[3]
                              + in.hvDuctWidthSide[4];
    // 高压平均宽/长（宽侧 B 列链全值 + 高压油道半值；长侧低压 3 油道 + 高压 3 油道半值）
    c.ab36Wide_mm = excelRound(
        M_PI * (in.coreDiameter_mm / 2.0 + B28 + c.lvRadial_mm + sumLvDuctH
                + c.mainDuct_mm
                + (c.hvRadial_mm + sumHvDuctH) / 2.0) * P12, 2);
    c.ab39Long_mm = excelRound(
        M_PI * (c.majorR_mm + B28 + c.lvRadial_mm
                + in.lvDuctWidthSide[0] + in.lvDuctWidthSide[1] + in.lvDuctWidthSide[2]
                + c.mainDuct_mm
                + (c.hvRadial_mm + in.hvDuctWidthSide[0] + in.hvDuctWidthSide[1]
                   + in.hvDuctWidthSide[2]) / 2.0) * S12, 2);
    // 低压平均宽/长（半值链）
    c.ak33Wide_mm = excelRound(
        M_PI * ((in.coreDiameter_mm / 2.0 + B28) + (c.lvRadial_mm + sumLvDuctH) / 2.0)
            * P12, 2);
    c.ak34Long_mm = excelRound(
        M_PI * ((c.majorR_mm + B28) + (c.lvRadial_mm + sumLvDuctW) / 2.0) * S12, 2);
    // 主空道平均宽/长
    c.ak36Wide_mm = excelRound(
        M_PI * (in.coreDiameter_mm / 2.0 + B28 + c.lvRadial_mm + sumLvDuctH
                + c.mainDuct_mm / 2.0) * P12, 2);
    c.ak39Long_mm = excelRound(
        M_PI * (c.majorR_mm + B28 + c.lvRadial_mm + sumLvDuctW
                + c.mainDuct_mm / 2.0) * S12, 2);

    c.hvMeanTurn_m = 2.0 * (c.ab36Wide_mm + c.ab39Long_mm) / 1000.0;  // X17
    c.lvMeanTurn_m = 2.0 * (c.ak33Wide_mm + c.ak34Long_mm) / 1000.0;  // AH17

    c.out->winding.hvRadial_mm = c.hvRadial_mm;
    c.out->winding.lvRadial_mm = c.lvRadial_mm;
    c.out->winding.mainDuct_mm = c.mainDuct_mm;
    c.out->winding.hvAxial_mm = c.hvAxial_mm;
    c.out->winding.hvMeanTurn_m = c.hvMeanTurn_m;
    c.out->winding.lvMeanTurn_m = c.lvMeanTurn_m;
    c.out->winding.layerCount = c.segTurnsPerLayer;
}

// ============================================================================
// 4. 窗高/中心距/油箱几何（计算单行 23..30/36..49）
// ============================================================================
void calcTankGeometry(EmCtx &c)
{
    const CalcInput &in = *c.in;
    const double B28 = in.phaseGapBase_mm;

    // 窗高 J14 = AA33
    c.windowHeight_mm = c.lvAxialOuter_mm;

    // 几何链（B29..B49）
    const double B29 = c.halfMinor_mm + B28;
    double b36 = B29 + c.lvRadial_mm
                 + in.lvDuctWidthSide[0] + in.lvDuctWidthSide[1]
                 + in.lvDuctWidthSide[2] + in.lvDuctWidthSide[3]
                 + in.lvDuctWidthSide[4];
    const double B38 = b36 + c.mainDuct_mm;
    const double B45 = B38 + c.hvRadial_mm
                       + in.hvDuctWidthSide[0] + in.hvDuctWidthSide[1]
                       + in.hvDuctWidthSide[2] + in.hvDuctWidthSide[3]
                       + in.hvDuctWidthSide[4];
    c.bChain_mm = B45;
    c.bChainFull_mm = B45 * 2.0;   // B47

    // 中心距 B49（椭圆形走第二分支）
    const double gap = (in.hvRated_kV < 12.0) ? 7.0
                       : (in.hvRated_kV < 24.0) ? 14.0 : 19.0;
    c.centerDist_mm = excelCeiling((c.bChainFull_mm + gap) / 5.0, 1.0) * 5.0 + 5.0;

    // 油箱宽 F25 = D51 + D52 + F24（侧净空）
    const double d51 = excelRound(
        in.coreDiameter_mm + 2.0 * B28 + in.coreStraight_mm, 0);
    const double d52 = 2.0 * (c.lvRadial_mm
                              + in.lvDuctHeightSide[0] + in.lvDuctHeightSide[1]
                              + in.lvDuctHeightSide[2] + c.mainDuct_mm + c.hvRadial_mm
                              + in.hvDuctHeightSide[0] + in.hvDuctHeightSide[1]
                              + in.hvDuctHeightSide[2]);
    c.tankWidth_mm = d51 + d52 + in.tankSideClear_mm;

    // 油箱长 H26 = H23 + H24 + H25 = 2×中心距 + B47（绕组辐向全链×2）+ 端净空
    c.tankLength_mm = 2.0 * c.centerDist_mm + c.bChainFull_mm + in.tankEndClear_mm;

    // 油箱高 J27 = 箱底油空 + 铁芯宽×2（J24 = G17×2）+ 窗高 + 垫脚
    const double j24 = c.lamWidth[0] * 2.0;
    c.tankHeight_mm = in.tankBottomOil_mm + j24 + c.windowHeight_mm + in.tankFoot_mm;

    // 波纹参数（S44..S49）
    c.waveThick_mm = (in.capacity_kVA >= 1000.0) ? 1.5 : 1.2;
    c.waveLongSides = excelInt((c.tankLength_mm - 90.0) / in.wavePitch_mm) + 1;
    c.waveShortSides = excelInt((c.tankWidth_mm - 90.0) / in.wavePitch_mm) + 1;

    // 油道等效宽（M29..M38，阻抗用）
    for (int i = 0; i < 5; ++i) {
        c.m29_33[i] = excelRound(
            (in.lvDuctWidthSide[i] * c.ak34Long_mm
             + in.lvDuctHeightSide[i] * c.ak33Wide_mm)
                / (c.ak34Long_mm + c.ak33Wide_mm), 2);
        c.m34_38[i] = excelRound(
            (in.hvDuctWidthSide[i] * c.ab39Long_mm
             + in.hvDuctHeightSide[i] * c.ab36Wide_mm)
                / (c.ab39Long_mm + c.ab36Wide_mm), 2);
    }

    c.out->mass.windowHeight_mm = c.windowHeight_mm;
    c.out->mass.centerDistance_mm = c.centerDist_mm;
    c.out->mass.tankWidth_mm = c.tankWidth_mm;
    c.out->mass.tankLength_mm = c.tankLength_mm;
    c.out->mass.tankHeight_mm = c.tankHeight_mm;
}

// ============================================================================
// 5. 铁芯重量与空载性能（Sheet1 行 5..26）
// ============================================================================
void calcCoreWeights(EmCtx &c)
{
    const CalcInput &in = *c.in;
    const double O2 = c.windowHeight_mm;   // 窗高
    const double R2 = c.centerDist_mm;     // 中心距
    const double J2 = in.steelThickness_mm;
    const double U2 = in.seamCount;
    const double W2 = in.stackFactor;

    double sumI = 0.0, sumM = 0.0, sumQ = 0.0, sumU = 0.0, sumY = 0.0;
    double sumLegs = 0.0;   // P23 心柱共重 = I22+M22+Q22+U22
    double sumColAct = 0.0;   // I24 实际截面累计（片宽×每台叠厚）
    double sumYokeAct = 0.0;  // O24 实际截面累计
    for (int i = 0; i < 16; ++i) {
        const double B = c.lamWidth[i];
        const double C = c.yokeWidth[i];
        // Sheet1 D 列：首级 D5 = G18×2（中心级全叠厚），其余级为单侧叠厚
        const double D = (i == 0) ? 2.0 * c.stack[0] : c.stack[i];
        if (B <= 0.0 && D <= 0.0) {
            continue;
        }
        // 片数 L（Sheet1 L 列）：叠厚/接缝数/片厚，偶数化；首级 ROUNDUP
        double sheets;
        if (i == 0) {
            sheets = std::ceil(D / U2 / J2 / 2.0) * 2.0;
        } else if (i == 11) {
            sheets = excelRound(2.0 * D / U2 / J2 / 2.0, 0) * 2.0 + 2.0;   // 宽90片 +2
        } else if (i == 12) {
            sheets = excelRound(2.0 * D / U2 / J2 / 2.0, 0) * 2.0 - 1.0;   // 宽80片 -1
        } else if (i == 13) {
            sheets = excelRound(2.0 * D / U2 / J2 / 2.0, 0) * 2.0;         // 宽60片
        } else {
            sheets = excelRound(2.0 * D / U2 / J2 / 2.0, 0) * 2.0;
        }
        const double G = 2.0 * J2 * U2 * sheets;    // G = J2×H（H = 2×U2×L）
        const double F = O2 + 2.0 * (c.yokeStep[i] + C);   // 片长（A、C柱）
        const double J = O2 + 2.0 * c.yokeStep[i] + B;     // 片长（心柱1）
        const double K = J2 * sheets;
        const double V = 2.0 * R2 + B;              // 片长（铁轭）
        const double O2c = 2.0 * K;

        // 各重量段（I/M/Q/U/Y 列）
        const double Iw = excelRound((F - B) * B * G / 1e6 * W2 * 7.65, 2);
        const double Mw = excelRound((J * B - B * B / 2.0) * K / 1e6 * W2 * 7.65, 2);
        const double Qw = excelRound((J * B - B * B / 2.0 - 2.0 * 5.0 * 5.0)
                                         * O2c / 1e6 * W2 * 7.65, 2);
        const double Uw = excelRound((J * B - B * B / 2.0 - 2.0 * 10.0 * 10.0)
                                         * O2c / 1e6 * W2 * 7.65, 2);
        const double Yw = excelRound(((V - C) * C - B * B / 4.0)
                                         * G / 1e6 * W2 * 7.65, 2);
        sumI += Iw;
        sumM += Mw;
        sumQ += Qw;
        sumU += Uw;
        sumY += Yw;
        sumColAct += B * G;
        sumYokeAct += C * G;
    }
    sumLegs = sumI + sumM + sumQ + sumU;   // P23
    const double totalCore = excelRound(sumLegs + sumY, 0);   // E22

    c.out->core.coreLegsWeight_kg = sumLegs;
    c.out->core.yokesWeight_kg = sumY;
    c.out->core.coreWeight_kg = totalCore;

    // 实际截面（I24/O24）：片宽×每台叠厚累计 × 叠片系数/200
    c.coreAreaAct_cm2 = sumColAct * W2 / 200.0;
    c.yokeAreaAct_cm2 = sumYokeAct * W2 / 200.0;
    c.out->core.coreAreaActual_cm2 = c.coreAreaAct_cm2;
    c.out->core.yokeAreaActual_cm2 = c.yokeAreaAct_cm2;

    // 实际磁密（I25/O25：CEILING 到 0.001；F16 设计磁密保留在 fluxDensity_core_T）
    const double et = c.out->core.turnVoltage_V;
    const double bCore = excelCeiling(45.0 * et / c.coreAreaAct_cm2, 0.001);
    const double bYoke = excelCeiling(45.0 * et / c.yokeAreaAct_cm2, 0.001);
    c.out->core.fluxDensity_yoke_T = bYoke;

    // 单位铁损插值（I26/O26）
    DesignDatabase &db = DesignDatabase::instance();
    double wCore = 0.0, wYoke = 0.0, vaKg = 0.0;
    if (!db.steelLossPerKgInterp(in.steelGrade, bCore, wCore)
        || !db.steelLossPerKgInterp(in.steelGrade, bYoke, wYoke)) {
        c.fail(QStringLiteral("硅钢曲线中未找到牌号 %1 的损耗数据").arg(in.steelGrade));
        return;
    }
    c.out->core.coreLossPerKg_W = wCore;
    c.out->core.yokeLossPerKg_W = wYoke;

    // 空载损耗（T26）：工艺系数 × 单位铁损 × 分重
    c.out->core.noLoadLoss_W = excelRound(
        in.coreLossCraftCoef * wCore * sumLegs
            + in.coreLossCraftCoef * wYoke * sumY, 0);

    // 磁化容量插值（J16，用设计磁密 F16）与空载电流（R16）
    const double bDesign = excelRound(et * 450.0 / c.coreArea_cm2 / 10.0, 3);
    if (db.steelMagnetizationPerKgInterp(in.steelGrade, bDesign, vaKg)) {
        c.out->core.magCapacity_vaPerKg = excelRound(vaKg, 3);
    }
    c.out->core.noLoadCurrent_pct = excelRound(
        2.5 * totalCore * c.out->core.magCapacity_vaPerKg / in.capacity_kVA / 10.0, 2);
}

// ============================================================================
// 6. 导线长/电阻/损耗/导线重
// ============================================================================
void calcWindingLosses(EmCtx &c)
{
    const CalcInput &in = *c.in;

    // 导线长（W18/Z18/AH18）
    const double w18 = excelRound(c.hvTurnsMax * c.hvMeanTurn_m + 0.5, 1);
    const double z18 = excelRound(c.hvMeanTurn_m * c.hvTurnsRated + 0.5, 1);
    const double ah18 = excelRound(in.lvTurns * c.lvMeanTurn_m + 0.5, 1);

    // 电阻 75℃（X19/AH19）
    const double x19 = excelRound(0.02135 * z18
            / (c.hvWireSection_mm2 * in.hvParallelCount), 6);
    const double ah19 = excelRound(
        (in.lvCopperFoil ? 0.02207 : 0.0357) * ah18 / c.lvWireSection_mm2, 6);

    // 电阻损耗（Y20/AH20，用相电流）
    const double y20 = excelRound(3.0 * c.hvPhaseCurrent_A * c.hvPhaseCurrent_A * x19, 0);
    const double ah20 = excelRound(3.0 * c.lvPhaseCurrent_A * c.lvPhaseCurrent_A * ah19, 0);

    c.out->winding.hvWireLenMax_m = w18;
    c.out->winding.hvWireLenRated_m = z18;
    c.out->winding.lvWireLen_m = ah18;
    c.out->winding.hvResistance_ohm = x19;
    c.out->winding.lvResistance_ohm = ah19;
    c.out->winding.hvCopperLoss_W = y20;
    c.out->winding.lvCopperLoss_W = ah20;

    // 导线重（W21/Z21/AH21）
    const double rhoHv = in.lvCopperFoil ? 8.9 : 2.7;   // 高压材质由低压标志同源（全铜/全铝）
    const double w21 = excelRound(3.0 * w18 * c.hvWireSection_mm2
            * in.hvParallelCount * rhoHv / 1000.0, 0);
    const double z21 = excelRound(
        3.825 * (in.hvBareWidth_mm + in.hvBareThick_mm + 0.354)
            / c.hvWireSection_mm2 / 100.0 * w21 + w21, 0);
    const double ah21 = excelRound(
        ah18 * c.lvWireSection_mm2 * (in.lvCopperFoil ? 8.9 : 2.7) * 3.0 / 1000.0, 0);
    c.out->winding.hvWireWeight_kg = z21;
    c.out->winding.lvWireWeight_kg = ah21;
    c.out->winding.wireWeightTotal_kg = z21 + ah21;   // C10
}

// ============================================================================
// 7. 负载损耗（L10）与附加损耗
// ============================================================================
void calcLoadLoss(EmCtx &c, double lambda_mm, double hx_mm)
{
    const CalcInput &in = *c.in;
    const double y20 = c.out->winding.hvCopperLoss_W;

    // 高压附加损耗 %（AA45）与 W（AC45）
    const double roundCoef = excelRound(1.0 - lambda_mm / (hx_mm * M_PI), 2);
    const double aa45 = excelRound(
        3.8e-7 * std::pow(50.0 * c.hvTurnsMax * in.hvBareWidth_mm
                              * c.hvWireSection_mm2 * roundCoef / c.ac30_mm, 2.0), 2);
    const double ac45 = excelRound(y20 * aa45 / 100.0, 0);
    const double aj45 = in.lvExtraLoss_W;

    c.out->winding.hvExtraLossPct = aa45;
    c.out->winding.hvExtraLoss_W = ac45;

    // 负载损耗（L10）：ΣI²R + 附加损耗，×(1+杂散系数)
    const double h10 = y20 + c.out->winding.lvCopperLoss_W;
    const double i10 = ac45 + aj45;
    c.out->winding.loadLoss_W = excelRound(
        (h10 + (in.hvBareWidth_mm == in.hvBareThick_mm ? 0.0 : i10))
            * (1.0 + in.strayLossFactor), 0);
}

// ============================================================================
// 8. 阻抗电压（R40/R41/O42/Q30..Q35）
// ============================================================================
void calcImpedance(EmCtx &c)
{
    const CalcInput &in = *c.in;

    // 漏磁总厚 M39
    double m39 = c.lvRadial_mm;
    for (int i = 0; i < 5; ++i) {
        m39 += c.m29_33[i];
    }
    m39 += c.mainDuct_mm + c.hvRadial_mm;
    for (int i = 0; i < 5; ++i) {
        m39 += c.m34_38[i];
    }
    c.lambda_mm = excelRound(m39, 2);

    // 低压分匝段匝数（AF10..AJ10，油道全设时 INT(AH8/6)）
    const double nSeg = std::floor(double(in.lvTurns) / 6.0);   // 3

    // a2（R40）：低压油道折算厚（段匝比²加权）
    // 各项匝比为累计段匝 AF10/AH8、(AF10+AG10)/AH8、…（先累计再求比）
    double acc2 = c.lvRadial_mm / 3.0;
    double cumSeg = 0.0;
    for (int i = 0; i < 5; ++i) {
        cumSeg += nSeg;
        acc2 += c.m29_33[i] * std::pow(cumSeg / in.lvTurns, 2.0);
    }
    const double a2 = excelRound(acc2, 2);

    // a1（R41）：高压油道折算厚（剩余匝比²加权，按油道层序累计）
    double acc1 = c.hvRadial_mm / 3.0;
    double cumLayers = 0.0;
    const double tRated = c.hvTurnsRated;
    const double perLayer = c.segTurnsPerLayer;
    for (int i = 0; i < 5; ++i) {
        cumLayers += c.ductLayerIdx[i];
        acc1 += c.m34_38[i]
                * std::pow((tRated - cumLayers * perLayer) / tRated, 2.0);
    }
    const double a1 = excelRound(acc1, 2);

    // 漏磁面积 O42 = a2×低压匝长 + 主空道×平均匝长和 + a1×高压匝长
    const double q29 = excelRound(2.0 * c.ak36Wide_mm + 2.0 * c.ak39Long_mm, 1);
    const double leakArea = excelRound(
        a2 * c.lvMeanTurn_m * 1000.0 + c.mainDuct_mm * q29
            + a1 * c.hvMeanTurn_m * 1000.0, 2);

    // 电抗高（Q30..Q33）
    const double q30 = std::fabs(c.ac30_mm - in.lvFoilWidth_mm);
    const double q31 = std::max(c.ac30_mm, in.lvFoilWidth_mm);
    const double q32 = excelRound(
        1.0 + q30 / 2.0 / q31 * (1.0 + 0.5 * M_PI * q30 / c.lambda_mm), 2);
    const double q33 = excelRound((c.ac30_mm + in.lvFoilWidth_mm) / 2.0
                                      + c.lambda_mm / 3.0, 1);

    // 电抗压降 P43 与阻抗 Q35
    const double p43 = excelRound(
        q32 * 3.95 * c.lvPhaseCurrent_A * in.lvTurns * leakArea
            / c.out->core.turnVoltage_V / q33 / 1e5, 2);

    c.out->impedance.lambda_mm = c.lambda_mm;
    c.out->impedance.hx_mm = q33;
    c.out->impedance.a1 = a1;
    c.out->impedance.a2 = a2;
    c.out->impedance.leakArea_mm2 = leakArea;
    c.out->impedance.kx = q32;

    // 负载损耗（依赖 λ/hx）
    calcLoadLoss(c, c.lambda_mm, q33);
    const double q34 = excelRound(
        c.out->winding.loadLoss_W / in.capacity_kVA / 10.0, 2);
    const double q35 = excelRound(std::sqrt(q34 * q34 + p43 * p43), 2);

    c.out->impedance.resistanceDrop_pct = q34;
    c.out->impedance.reactanceDrop_pct = p43;
    c.out->impedance.impedance_pct = q35;
}

// ============================================================================
// 9. 温升（O44..O48/N49/AB48..AK52）
// ============================================================================
void calcThermal(EmCtx &c)
{
    const CalcInput &in = *c.in;
    const double H1 = in.capacity_kVA;
    const double H26 = c.tankLength_mm;
    const double F25 = c.tankWidth_mm;
    const double J27 = c.tankHeight_mm;
    const double S45 = in.waveDepth_mm;
    const double S46 = in.waveHeight_mm;
    const double S47 = in.wavePitch_mm;
    const double S48 = c.waveLongSides;
    const double S49 = c.waveShortSides;

    // 散热面积（波纹油箱，S49>0 双面布置）
    const double o44 = excelRound(0.75 * H26 * F25 / 1e6, 2);
    double ks = 0.628, kp = 0.35;
    DesignDatabase::instance().corrugatedCoefs(S45, ks, kp);
    double o45, o46;
    if (S49 > 0) {
        o45 = excelRound(2.0 * (S48 + S49 - 2.0) * (S47 - 6.0 - 2.0 * c.waveThick_mm
                                                    + 2.0 * S45) * S46 * ks / 1e6, 2);
        o46 = excelRound(8.0 * S45 * S46 / 1e6, 2);
    } else {
        o45 = excelRound(2.0 * (S48 - 1.0) * (S47 - 6.0 - 2.0 * c.waveThick_mm
                                               + 2.0 * S45) * S46 * ks / 1e6, 2);
        o46 = excelRound(4.0 * S45 * S46 / 1e6, 2);
    }
    const double o47 = excelRound(
        (((H26 + F25) * 2.0 - 2.0 * 45.0 * (S49 > 0 ? S48 + S49 - 2.0 : S48 - 1.0)) * J27
         + 2.0 * 45.0 * (S49 > 0 ? S48 + S49 - 2.0 : S48 - 1.0) * (J27 - S46)) / 1e6, 2);
    const double o48 = o44 + o45 + o46 + o47;

    // 油面温升（I49/N49）与油顶层（N51）
    const double i49 = excelRound(
        (c.out->winding.loadLoss_W + c.out->core.noLoadLoss_W) / o48, 0);
    const double n49 = excelRound(0.262 * std::pow(i49, 0.8), 1);
    const double n51 = excelRound(1.2 * n49 + 6.0, 1);

    // 绕组散热面积（AC47 高压 / AK47 低压）
    const double P12 = c.majorAngle_deg / 180.0;   // 角度分数 144/180
    const double S12 = c.minorAngle_deg / 180.0;   // 角度分数 36/180
    const double B28 = in.phaseGapBase_mm;
    const double B29 = c.halfMinor_mm + B28;
    const double ductTermHv = [&]() {
        double s = 0.0;
        for (int i = 0; i < 5; ++i) {
            if (in.hvDuctWidthSide[i] > 0.0) {
                s += c.ab39Long_mm;
            }
            if (in.hvDuctHeightSide[i] > 0.0) {
                s += c.ab36Wide_mm;
            }
        }
        return s;
    }();
    const double sumLvDuctH = in.lvDuctHeightSide[0] + in.lvDuctHeightSide[1]
                              + in.lvDuctHeightSide[2] + in.lvDuctHeightSide[3]
                              + in.lvDuctHeightSide[4];
    const double sumLvDuctW = in.lvDuctWidthSide[0] + in.lvDuctWidthSide[1]
                              + in.lvDuctWidthSide[2] + in.lvDuctWidthSide[3]
                              + in.lvDuctWidthSide[4];
    const double sumHvDuctH = in.hvDuctHeightSide[0] + in.hvDuctHeightSide[1]
                              + in.hvDuctHeightSide[2] + in.hvDuctHeightSide[3]
                              + in.hvDuctHeightSide[4];
    const double sumHvDuctW = in.hvDuctWidthSide[0] + in.hvDuctWidthSide[1]
                              + in.hvDuctWidthSide[2] + in.hvDuctWidthSide[3]
                              + in.hvDuctWidthSide[4];
    const double perimHv = excelRound(
        M_PI * (B29 + c.lvRadial_mm + sumLvDuctH + c.mainDuct_mm + c.hvRadial_mm
                + sumHvDuctH) * P12
            + M_PI * (c.majorR_mm + B28 + c.lvRadial_mm + sumLvDuctW
                      + c.mainDuct_mm + c.hvRadial_mm + sumHvDuctW) * S12, 1);
    const double ac47 = excelRound(
        6.0 * c.ac30_mm * (ductTermHv * 1.7 + perimHv) / 1e6, 2);

    const double ductTermLv = [&]() {
        double s = 0.0;
        for (int i = 0; i < 5; ++i) {
            if (in.lvDuctWidthSide[i] > 0.0) {
                s += c.ak34Long_mm;
            }
            if (in.lvDuctHeightSide[i] > 0.0) {
                s += c.ak33Wide_mm;
            }
        }
        return s;
    }();
    const double ak47 = excelRound(
        6.0 * in.lvFoilWidth_mm
            * (ductTermLv * 1.7
               + (c.mainDuct_mm < 4.0 ? 0.0
                                      : (c.ak36Wide_mm + c.ak39Long_mm) * 0.85)) / 1e6, 2);

    // 热负荷与绕组温升（AB48/AC49/AC52 → Y53/Y54；AK48/AK49/AK50 → AK51/AK52）
    const double ab48 = excelRound(
        (c.out->winding.hvCopperLoss_W + c.out->winding.hvExtraLoss_W) / ac47, 1);
    const double ak48 = excelRound(
        (c.out->winding.lvCopperLoss_W + in.lvExtraLoss_W) / ak47, 2);
    const double ac49 = excelRound(0.065 * std::pow(ab48, 0.8), 1);
    const double ak49 = excelRound(0.065 * std::pow(ak48, 0.8), 1);

    // 层间油升修正（AC52/AK50）：gap = 层间绝缘均摊 + 漆膜厚
    const double layerGap = excelRound(
        c.hvLayerInsulTotal_mm / (in.hvTurnsPerLayer - 1)
            + c.hvInsWidth_mm - in.hvBareWidth_mm, 2);
    const int hvDuctCountY = (in.hvDuctHeightSide[0] > 0 ? 2 : 0)
                             + (in.hvDuctHeightSide[1] > 0 ? 2 : 0)
                             + (in.hvDuctHeightSide[2] > 0 ? 2 : 0);
    const double coef52 = in.hvTurnsPerLayer - 2.0 * (0 + hvDuctCountY + 1);
    const double ac52 = excelRound(
        0.002 * std::min(layerGap, 0.64) * coef52 * ab48, 1);

    int lvDuctCount = 0;
    for (int i = 0; i < 5; ++i) {
        if (in.lvDuctHeightSide[i] > 0.0) {
            ++lvDuctCount;
        }
    }
    const double ak50 = excelRound(
        0.002 * in.lvLayerInsulCount * in.lvLayerInsul_mm
            * (in.lvTurns - 2.0 * (lvDuctCount * 2.0 + 1.0)) * ak48, 1);

    const double y53 = ac49 + ac52;
    const double y54 = y53 + n49;
    const double ak51 = ak49 + std::max(ak50, 0.0);
    const double ak52 = ak51 + n49;

    c.out->thermal.tankSurface_m2 = o44;
    c.out->thermal.corrSurface_m2 = o45;
    c.out->thermal.topSurface_m2 = o47;
    c.out->thermal.totalSurface_m2 = o48;
    c.out->thermal.oilRise_K = n49;
    c.out->thermal.oilTopRise_K = n51;
    c.out->thermal.hvWindingRise_K = y54;
    c.out->thermal.lvWindingRise_K = ak52;
    c.out->thermal.hvHeatLoad = ab48;
    c.out->thermal.lvHeatLoad = ak48;
}

// ============================================================================
// 10. 重量与成本（C10..C25 + 成本测算）
// ============================================================================
void calcMassCost(EmCtx &c)
{
    const CalcInput &in = *c.in;
    const double H1 = in.capacity_kVA;
    const double kV = in.hvRated_kV;

    // 箱盖/箱沿/箱壁/箱底厚度（C12..C15）
    const double c12raw = (kV <= 12.0)
        ? ((H1 <= 315.0) ? 6.0 : (H1 <= 1000.0 ? 8.0 : 10.0))
        : ((H1 <= 200.0) ? 8.0 : (H1 <= 1000.0 ? 10.0 : 12.0));
    const double c12 = c12raw - 2.0;
    const double q22 = 75.0;   // 箱沿高 Q22（波纹油箱）
    const double c14 = (kV <= 12.0) ? ((H1 <= 1000.0) ? 4.0 : 6.0)
                                    : ((H1 <= 630.0) ? 4.0 : 6.0);
    const double c15 = (kV <= 12.0)
        ? ((H1 <= 315.0) ? 4.0 : (H1 <= 1000.0 ? 6.0 : 8.0))
        : ((H1 <= 200.0) ? 4.0 : (H1 <= 1000.0 ? 6.0 : 10.0));

    const double H26 = c.tankLength_mm;
    const double F25 = c.tankWidth_mm;
    const double J27 = c.tankHeight_mm;
    const double S45 = in.waveDepth_mm;
    const double S46 = in.waveHeight_mm;
    const double S47 = in.wavePitch_mm;
    const double S48 = c.waveLongSides;
    const double S49 = c.waveShortSides;

    // 油箱分件重（D12..D17）
    const double d12 = excelRound((H26 + 200.0) * (F25 + 200.0) * c12 * 7.85 / 1e6, 0);
    const double coef13 = (H1 < 630.0) ? 5.699 : (H1 < 1000.0 ? 9.878 : 12.142);
    const double d13 = excelRound(2.0 * (H26 + F25 + 2.0 * q22) / 1000.0 * coef13, 0);
    const double q26 = J27 - S46 - q22;   // Q26 油箱顶空
    const double d14 = (S49 > 0)
        ? excelRound(2.0 * (H26 + F25) * (q26 + 15.0) * c14 * 7.85 / 1e6, 0)
        : excelRound(2.0 * (H26 + F25) * J27 * c14 * 7.85 / 1e6, 0);
    const double d15 = excelRound(F25 * H26 * c15 * 7.85 / 1e6, 0);
    const double d16 = 1.15;
    double d17;
    if (S49 > 0) {
        d17 = excelRound(2.0 * ((S48 + S49 - 2.0) * 45.0 + (S48 + S49) * 2.0 * S45
                                + 2.0 * (F25 + H26 - (S48 + S49) * 45.0))
                             * S46 * c.waveThick_mm * 7.85 / 1e6, 0);
    } else {
        d17 = excelRound(2.0 * ((S48 - 1.0) * S47 + S48 * 2.0 * S45 + 2.0 * F25
                                + 2.0 * (H26 - 45.0 * S48))
                             * S46 * c.waveThick_mm * 7.85 / 1e6, 0);
    }
    const double tankWeight = excelRound(
        (d12 + d13 + d14 + d15) * d16 + d17, 0);   // C19

    // 油重（C20..C24）
    const double c20 = excelRound(H26 * F25 * J27 * 0.9 / 1e6, 0);
    const double steelW = c.out->core.coreWeight_kg;
    const double hvW = c.out->winding.hvWireWeight_kg;
    const double lvW = c.out->winding.lvWireWeight_kg;
    const double c21 = excelRound(
        steelW / 7.8 + (in.lvCopperFoil ? hvW / 4.5 : hvW / 1.85)
            + (in.lvCopperFoil ? lvW / 4.5 : lvW / 1.85), 0);
    const double c22 = excelRound(
        6.0 * S46 * S45 * 0.9 * (S48 + S49) * 2.0 / 1e6, 0);
    const double oilWeight = c20 - c21 + c22;   // C24

    // 器身重与总重（C11/C25）
    const double ratio = in.lvCopperFoil ? 1.15 : 1.20;   // 全铜 1.15
    const double activePart = excelRound(
        ratio * (steelW + c.out->winding.wireWeightTotal_kg), 0);
    const double totalWeight = tankWeight + oilWeight + activePart;

    c.out->mass.tankWeight_kg = tankWeight;
    c.out->mass.oilWeight_kg = oilWeight;
    c.out->mass.activePartWeight_kg = activePart;
    c.out->mass.totalWeight_kg = totalWeight;

    // 材料成本（成本测算表，单价按计算单默认）
    const double cuPrice = 60.0;
    c.out->cost.steelCost = excelRound(steelW * 1.05, 0) * 17.0;
    c.out->cost.hvWireCost = excelRound(hvW * 1.05, 0) * (cuPrice + 3.3);
    c.out->cost.lvWireCost = excelRound(lvW * 1.08, 0) * (cuPrice * 1.05 + 6.5);
    c.out->cost.oilCost = excelRound(oilWeight * 1.1, 0) * 10.0;
    c.out->cost.tankCost = (excelRound(tankWeight * 1.05, 0) + 200.0) * 9.0;
    c.out->cost.materialCost = c.out->cost.steelCost + c.out->cost.hvWireCost
                               + c.out->cost.lvWireCost + c.out->cost.oilCost
                               + c.out->cost.tankCost;
}

}  // namespace

// ============================================================================
// 对外接口
// ============================================================================
bool ElectromagneticEngine::calcElectromagnetic(const CalcInput &input, CalcResult &result)
{
    result = CalcResult();

    // 基础数据表（硅钢曲线/叠积表/线规）懒加载：首次调用时从 qrc 资源读取
    DesignDatabase &db = DesignDatabase::instance();
    if (!db.isLoaded() && !db.load()) {
        result.error = QStringLiteral("基础数据表加载失败: %1").arg(db.lastError());
        return false;
    }

    EmCtx ctx;
    ctx.in = &input;
    ctx.out = &result;

    calcElectrical(ctx);
    if (ctx.failed) {
        result.error = ctx.error;
        return false;
    }
    calcCoreGeometry(ctx);
    if (ctx.failed) {
        result.error = ctx.error;
        return false;
    }
    calcWindingLayout(ctx);
    calcTankGeometry(ctx);
    calcCoreWeights(ctx);
    if (ctx.failed) {
        result.error = ctx.error;
        return false;
    }
    calcWindingLosses(ctx);
    calcImpedance(ctx);
    calcThermal(ctx);
    calcMassCost(ctx);

    result.valid = !ctx.failed;
    result.error = ctx.error;
    return result.valid;
}

PrintOutputData ElectromagneticEngine::calculate(const TransformerParams &params,
                                                 const StructureConfig &config)
{
    Q_UNUSED(params);
    Q_UNUSED(config);
    // 旧打印接口：以默认设计变量（SB20-M-630-10）执行电磁计算并输出关键结果
    CalcInput input;
    CalcResult result;
    calcElectromagnetic(input, result);
    return buildPrintOutput(input, result);
}

PrintOutputData ElectromagneticEngine::buildPrintOutput(const CalcInput &input,
                                                        const CalcResult &result)
{
    Q_UNUSED(input);
    PrintOutputData data;
    if (!result.valid) {
        PrintOutputRow row;
        row.isSectionHeader = true;
        row.leftName = QStringLiteral("电磁计算失败");
        row.leftValue = result.error;
        data.rows.append(row);
        return data;
    }
    auto addRow = [&data](const QString &ln, const QString &lv, const QString &lu,
                          const QString &rn = QString(), const QString &rv = QString(),
                          const QString &ru = QString()) {
        PrintOutputRow row;
        row.leftName = ln;
        row.leftValue = lv;
        row.leftUnit = lu;
        row.rightName = rn;
        row.rightValue = rv;
        row.rightUnit = ru;
        data.rows.append(row);
    };
    addRow(QStringLiteral("铁芯截面"), QString::number(result.core.coreArea_cm2, 'f', 2),
           QStringLiteral("cm²"),
           QStringLiteral("磁密"), QString::number(result.core.fluxDensity_core_T, 'f', 3),
           QStringLiteral("T"));
    addRow(QStringLiteral("硅钢片重"), QString::number(result.core.coreWeight_kg, 'f', 0),
           QStringLiteral("kg"),
           QStringLiteral("空载损耗"), QString::number(result.core.noLoadLoss_W, 'f', 0),
           QStringLiteral("W"));
    addRow(QStringLiteral("负载损耗"), QString::number(result.winding.loadLoss_W, 'f', 0),
           QStringLiteral("W"),
           QStringLiteral("阻抗电压"), QString::number(result.impedance.impedance_pct, 'f', 2),
           QStringLiteral("%"));
    addRow(QStringLiteral("油面温升"), QString::number(result.thermal.oilRise_K, 'f', 1),
           QStringLiteral("K"),
           QStringLiteral("高压绕组温升"),
           QString::number(result.thermal.hvWindingRise_K, 'f', 1), QStringLiteral("K"));
    addRow(QStringLiteral("器身重"), QString::number(result.mass.activePartWeight_kg, 'f', 0),
           QStringLiteral("kg"),
           QStringLiteral("总重"), QString::number(result.mass.totalWeight_kg, 'f', 0),
           QStringLiteral("kg"));
    addRow(QStringLiteral("材料成本"), QString::number(result.cost.materialCost, 'f', 0),
           QStringLiteral("元"));
    return data;
}

// ============================================================================
// 对拍自检：SB20-M-630-10 计算单缓存值对照
// ============================================================================
QString ElectromagneticEngine::selfTestReport()
{
    struct CheckItem {
        const char *name;
        double actual;
        double expect;
        double tol;
    };
    CalcInput in;
    CalcResult r;
    ElectromagneticEngine engine;
    if (!engine.calcElectromagnetic(in, r)) {
        return QStringLiteral("电磁计算失败: %1").arg(r.error);
    }
    const CheckItem items[] = {
        { "匝电压 AC4",           r.core.turnVoltage_V,        12.8333, 0.0001 },
        { "短轴长 L20",           r.core.minorAxis_mm,         186.0,  0.5 },
        { "心柱截面 F14",         r.core.coreArea_cm2,         375.44, 0.2 },
        { "铁轭截面 J23",         r.core.yokeArea_cm2,         381.52, 0.2 },
        { "磁密 F16",             r.core.fluxDensity_core_T,   1.538,  0.002 },
        { "硅钢片重 N14",         r.core.coreWeight_kg,        905.0,  1.0 },
        { "空载损耗 O16",         r.core.noLoadLoss_W,         529.0,  6.0 },
        { "空载电流 R16",         r.core.noLoadCurrent_pct,    0.6,    0.05 },
        { "高压匝数 Y8",          double(r.winding.hvTurnsRated), 779.0, 0.0 },
        { "高压辐向 W28",         r.winding.hvRadial_mm,       39.0,   0.1 },
        { "低压辐向 AK23",        r.winding.lvRadial_mm,       28.5,   0.1 },
        { "高压平均匝长 X17",     r.winding.hvMeanTurn_m,      1.25118, 0.002 },
        { "低压平均匝长 AH17",    r.winding.lvMeanTurn_m,      0.90686, 0.002 },
        { "高压电阻 X19",         r.winding.hvResistance_ohm,  1.900897, 0.002 },
        { "负载损耗 L10",         r.winding.loadLoss_W,        5206.0, 15.0 },
        { "漏磁面积 O42",         r.impedance.leakArea_mm2,    48064.73, 50.0 },
        { "阻抗电压 Q35",         r.impedance.impedance_pct,   6.93,   0.05 },
        { "中心距 B49",           r.mass.centerDistance_mm,    425.0,  0.5 },
        { "油箱长 H26",           r.mass.tankLength_mm,        1323.0, 1.0 },
        { "油箱宽 F25",           r.mass.tankWidth_mm,         556.0,  1.0 },
        { "油面温升 N49",         r.thermal.oilRise_K,         26.7,   0.5 },
        { "高压绕组温升 Y54",     r.thermal.hvWindingRise_K,   31.9,   0.5 },
        { "总油重 C24",           r.mass.oilWeight_kg,         537.0,  3.0 },
        { "油箱及附件 C19",       r.mass.tankWeight_kg,        410.0,  3.0 },
        { "变压器总重 C25",       r.mass.totalWeight_kg,       2566.0, 8.0 },
    };
    QString report;
    int pass = 0, total = 0;
    for (const auto &it : items) {
        ++total;
        const bool ok = std::fabs(it.actual - it.expect) <= it.tol;
        if (ok) {
            ++pass;
        }
        report += QStringLiteral("%1 [%2] 计算=%3 期望=%4\n")
                      .arg(QString::fromUtf8(it.name),
                           ok ? QStringLiteral("通过") : QStringLiteral("偏差"),
                           QString::number(it.actual, 'f', 4),
                           QString::number(it.expect, 'f', 4));
    }
    report.prepend(QStringLiteral("SB20-M-630-10 对拍：%1/%2 通过\n")
                       .arg(pass).arg(total));
    return report;
}
