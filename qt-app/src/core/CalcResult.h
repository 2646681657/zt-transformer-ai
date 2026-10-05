#ifndef CALCRESULT_H
#define CALCRESULT_H
// 电磁计算结果：与 SB20 计算单输出项一一对应
// 单元格标注（如 O16）为 SB20-M-630-10 计算单中的缓存值位置

#include <QString>
#include <QVector>
#include "TestVoltageHints.h"

// AM8→AN8，仅建议，不参与实际匝数的写回。
struct LvTurnsRecommendation {
    double referenceFlux_T = 0.0;
    double coreArea_cm2 = 0.0;
    double phaseVoltage_V = 0.0;
    double referenceTurnVoltage_V = 0.0;
    int turns = 0; // 0表示不可用，原因见error
    QString error;
};

// ---- 铁芯（叠积/磁密/空载）----
struct CoreResult {
    bool yokePiece1Auto = false;
    double yokePiece1Stack_mm = 0.0; // 实际采用的Sheet1 D16
    double yokePiece1Width_mm = 0.0; // 实际F19（基宽90+叠积表偏移）
    LvTurnsRecommendation lvTurnsRecommendation;
    double turnVoltage_V = 0.0;        // AC4 匝电压
    // 椭圆几何
    double majorRadius_mm = 0.0;       // M20 大圆半径
    double yokeFlat_mm = 0.0;          // O20 圆心到轴
    double junctionHeight_mm = 0.0;    // R20 交接点高
    double minorAxis_mm = 0.0;         // L20 短轴长
    // 叠积（16 级：11 级主叠积 + 轭片基宽 90/80/60 + 空）
    QVector<double> widths_mm;         // 各级片宽
    QVector<double> stacks_mm;         // 各级叠厚
    double coreArea_cm2 = 0.0;         // F14/D23 心柱截面
    double yokeArea_cm2 = 0.0;         // J23 铁轭截面
    double coreAreaActual_cm2 = 0.0;   // I24 实际心柱截面
    double yokeAreaActual_cm2 = 0.0;   // O24 实际铁轭截面
    double fluxDensity_core_T = 0.0;   // F16/I25 心柱磁密
    double fluxDensity_yoke_T = 0.0;   // O25 铁轭磁密
    // 重量
    double coreLegsWeight_kg = 0.0;    // P23 三相心柱共重
    double yokesWeight_kg = 0.0;       // U23 上下铁轭共重
    double coreWeight_kg = 0.0;        // N14/E22 硅钢片总重
    // 空载性能
    double coreLossPerKg_W = 0.0;      // I26 心柱单位铁损（插值）
    double yokeLossPerKg_W = 0.0;      // O26 铁轭单位铁损（插值）
    double coreLossCraftCoef = 0.0;    // 实际采用的心柱系数
    double yokeLossCraftCoef = 0.0;    // 实际采用的铁轭系数
    double coreLegsLoss_W = 0.0;       // 心柱铁损分项（未取整）
    double yokesLoss_W = 0.0;          // 铁轭铁损分项（未取整）
    double noLoadLoss_W = 0.0;         // O16/T26 空载损耗
    double magCapacity_vaPerKg = 0.0;  // J16 磁化容量（插值）
    double noLoadCurrent_pct = 0.0;    // R16 空载电流 %
};

// ---- 绕组（尺寸/导线/损耗）----
struct WindingResult {
    bool hvRoundWire = false;         // 本次实际导线分支快照
    double hvRoundWireDiameter_mm = 0.0;
    double hvRoundWeightAddPct = 0.0;  // 圆线表Q列绝缘增重百分比
    QString oilDuctLayoutNote;        // 布局提示快照，非散热合格判定
    // 匝数
    int hvTurnsMax = 0;                // V8 最高分接匝数
    int hvTurnsRated = 0;              // Y8 额定匝数
    int hvTurnsMin = 0;                // AA8 最低分接匝数
    int lvTurns = 0;                   // AH8 低压匝数
    // 层分布
    int layerCount = 0;                // Y9 高压每层匝数；保留旧字段名，非W12总层数
    int ductLayerIdx[6] = {0, 0, 0, 0, 0, 0};  // Z30..Z34 前油道层序（0=无）
    // 导线
    QString hvWireInsulation;
    double hvWireInsulAdd_mm = 0.0;
    double hvInsWidth_mm = 0.0;        // X14 绝缘导线宽
    double hvInsThick_mm = 0.0;        // Z14 绝缘导线厚
    double hvWireSection_mm2 = 0.0;    // U15 高压单根导线截面（保留原字段口径）
    double hvEffectiveSection_mm2 = 0.0; // AA15 单根截面×并绕×叠绕
    double lvWireSection_mm2 = 0.0;    // AH15 低压箔截面
    double hvCurrentDensity = 0.0;     // X16 高压电密
    double lvCurrentDensity = 0.0;     // AH16 低压电密
    // 辐向/轴向
    double lvRadial_mm = 0.0;          // AK23 低压辐向厚
    double hvRadial_mm = 0.0;          // W28 高压辐向厚
    double mainDuct_mm = 0.0;          // AK43 主空道
    double hvAxial_mm = 0.0;           // AA28 高压轴向高
    int hvCoilFormIdx = 1;             // 本次计算采用的AC9快照
    double hvSegmentGap_mm = 0.0;      // AA29 段间距
    double hvTotalAxial_mm = 0.0;      // AA30 = AA28 + AA29
    double hvInnerAxial_mm = 0.0;      // AC30 = ROUND(AA30-AA22,2)
    double hvEndInsul_mm = 0.0;        // AA31 = (AA33-AA30)/2；仅展示，不作合格判定
    double hvEndInsulReference_mm = 0.0; // AC32 原表参考下限
    double lvAxial_mm = 0.0;           // AK23 低压轴向（箔宽+端绝缘）
    // 平均匝长与导线长
    double hvMeanTurn_m = 0.0;         // X17 高压平均匝长
    double lvMeanTurn_m = 0.0;         // AH17 低压平均匝长
    double hvWireLenMax_m = 0.0;       // W18 高压导线长（最大分接）
    double hvWireLenRated_m = 0.0;     // Z18 额定匝导线长
    double lvWireLen_m = 0.0;          // AH18 低压导线长
    // 电阻（75℃）
    double hvResistance_ohm = 0.0;     // X19
    double lvResistance_ohm = 0.0;     // AH19
    // 损耗
    double recordedLeadLoss_W = 0.0;  // 本次计算输入快照，仅记录，未计入损耗或温升
    double hvCopperLoss_W = 0.0;       // Y20 高压电阻损耗
    double lvCopperLoss_W = 0.0;       // AH20 低压电阻损耗
    double hvExtraLossPct = 0.0;       // AA45 高压附加损耗 %
    double hvExtraLoss_W = 0.0;        // AC45 高压附加损耗 W
    double lvExtraLoss_W = 0.0;        // 输入快照：圆线不计入L10，仍参与低压热负荷
    double strayLossFactor = 0.0;      // J10 杂散系数快照，如0.11，不是百分数11
    double loadLossBeforeStray_W = 0.0; // 杂散修正前合计，保留实际导线分支，未另行取整
    double loadLoss_W = 0.0;           // L10 负载损耗
    // 导线重
    double hvBareWireWeight_kg = 0.0;  // W21 高压裸导线重（三相合计）
    double hvWireWeight_kg = 0.0;      // Z21 高压导线重
    double lvWireWeight_kg = 0.0;      // AH21 低压导线重
    double wireWeightTotal_kg = 0.0;   // C10 导线总重
};

// ---- 阻抗电压 ----
struct ImpedanceResult {
    double lambda_mm = 0.0;            // M39 漏磁通道总厚 λ
    double hx_mm = 0.0;                // Q33 绕组电抗高
    double axialDifference_mm = 0.0;   // Q30 = ABS(AC30-AJ14)+AA29
    double a1 = 0.0;                   // R41 高压漏磁折算厚
    double a2 = 0.0;                   // R40 低压漏磁折算厚
    double leakArea_mm2 = 0.0;         // O42 Sx 漏磁面积
    double kx = 0.0;                   // Q32 横向漏磁系数
    double resistanceDrop_pct = 0.0;   // Q34 电阻压降 %
    double reactanceDrop_pct = 0.0;    // P43 电抗压降 %
    double impedance_pct = 0.0;        // Q35 阻抗电压 %
};

// ---- 温升 ----
struct ThermalResult {
    double tankSurface_m2 = 0.0;       // O44 箱壁散热面积
    double corrSurface_m2 = 0.0;       // O45 波纹散热面积
    double topSurface_m2 = 0.0;        // O47 箱顶散热面积
    double totalSurface_m2 = 0.0;      // O48 总散热面积
    double oilRise_K = 0.0;            // N49 油面温升
    double oilTopRise_K = 0.0;         // N51 油顶层温升
    double hvWindingRise_K = 0.0;      // Y54 高压绕组温升
    double lvWindingRise_K = 0.0;      // AK52 低压绕组温升
    double hvHeatLoad = 0.0;           // AB48 高压热负荷
    double lvHeatLoad = 0.0;           // AK48 低压热负荷
    double hvSurface_m2 = 0.0;         // AC47 高压散热面积
    double hvEffectiveHeight_mm = 0.0; // AC47高度因子，AC9=2时为AC30-AA29
    double lvSurface_m2 = 0.0;         // AK47 低压散热面积
    double hvLayerGap_mm = 0.0;        // ROUND(W25/(W12-1)+X14-X13,2)
    double mainDuctSecond_mm = 0.0;    // AG43 主空道第二油道
    double hvSurfaceRise_K = 0.0;      // AC49 表面温升
    double hvGapCorrection_K = 0.0;    // AC51 大间隙修正，原表为空时按0
    double hvLayerCorrection_K = 0.0;  // AC52 原始修正，允许负值
    double hvRiseAboveOil_K = 0.0;     // Y53 对油温升（不叠加负AC52）
    double lvSurfaceRise_K = 0.0;      // AK49 低压表面温升
    double lvLayerCorrection_K = 0.0;  // AK50 原始层间修正，允许负值
    double lvRiseAboveOil_K = 0.0;     // AK51 低压对油温升（不叠加负AK50）
};

// ---- 重量与成本 ----
struct MassResult {
    double windowHeight_mm = 0.0;      // J14 窗高
    double centerDistance_mm = 0.0;    // B49/L14 中心距
    double activePartWeight_kg = 0.0;  // C11 器身重
    double tankWidth_mm = 0.0;         // F25 油箱宽
    double tankLength_mm = 0.0;        // H26 油箱长
    double tankHeight_mm = 0.0;        // J27 油箱高
    double tankWeight_kg = 0.0;        // C19 油箱及附件重
    double oilWeight_kg = 0.0;         // C24 总油重
    double totalWeight_kg = 0.0;       // C25 变压器总重
};

struct CostResult {
    double steelCost = 0.0;            // 硅钢片成本
    double hvWireCost = 0.0;           // 高压导线成本
    double lvWireCost = 0.0;           // 低压箔成本
    double oilCost = 0.0;              // 绝缘油成本
    double tankCost = 0.0;             // 油箱成本
    double materialCost = 0.0;         // 材料成本合计（不含钢材等未翻译项）
};

// 原计算单N53/N54/P54的独立参考，不参与温升合格或寻优筛选。
struct OilExpansionResult {
    bool available = false;
    bool passed = false;
    double oilWeight_kg = 0.0;
    double demand_kg = 0.0;
    double capacity_kg = 0.0;
    double margin_kg = 0.0;
    double waveDepth_mm = 0.0;
    double waveHeight_mm = 0.0;
    double longSideCount = 0.0;
    double shortSideCount = 0.0;
    double kp = 0.0;
    double expansionCoefficient = 0.0007;
    double referenceDeltaT_K = 50.0;
    QString error = QStringLiteral("尚未生成膨缩校核");
    QString status() const {
        return !available ? QStringLiteral("需人工核对")
            : passed ? QStringLiteral("合格（参考）") : QStringLiteral("不合格（参考）");
    }
};

struct CalcResult {
    OilExpansionResult oilExpansion;
    TestVoltageHints testVoltage;
    CoreResult core;
    WindingResult winding;
    ImpedanceResult impedance;
    ThermalResult thermal;
    MassResult mass;
    CostResult cost;
    bool valid = false;                // 全链路是否计算成功
    QString error;                     // 失败原因
};

#endif  // CALCRESULT_H
