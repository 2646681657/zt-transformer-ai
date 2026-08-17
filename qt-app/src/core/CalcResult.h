#ifndef CALCRESULT_H
#define CALCRESULT_H
// 电磁计算结果：与 SB20 计算单输出项一一对应
// 单元格标注（如 O16）为 SB20-M-630-10 计算单中的缓存值位置

#include <QString>
#include <QVector>

// ---- 铁芯（叠积/磁密/空载）----
struct CoreResult {
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
    double noLoadLoss_W = 0.0;         // O16/T26 空载损耗
    double magCapacity_vaPerKg = 0.0;  // J16 磁化容量（插值）
    double noLoadCurrent_pct = 0.0;    // R16 空载电流 %
};

// ---- 绕组（尺寸/导线/损耗）----
struct WindingResult {
    // 匝数
    int hvTurnsMax = 0;                // V8 最高分接匝数
    int hvTurnsRated = 0;              // Y8 额定匝数
    int hvTurnsMin = 0;                // AA8 最低分接匝数
    int lvTurns = 0;                   // AH8 低压匝数
    // 层分布
    int layerCount = 0;                // Y9 每段层数
    int ductLayerIdx[6] = {0, 0, 0, 0, 0, 0};  // Z30..Z34 前油道层序（0=无）
    // 导线
    double hvWireSection_mm2 = 0.0;    // AA15 高压导线截面
    double lvWireSection_mm2 = 0.0;    // AH15 低压箔截面
    double hvCurrentDensity = 0.0;     // X16 高压电密
    double lvCurrentDensity = 0.0;     // AH16 低压电密
    // 辐向/轴向
    double lvRadial_mm = 0.0;          // AK23 低压辐向厚
    double hvRadial_mm = 0.0;          // W28 高压辐向厚
    double mainDuct_mm = 0.0;          // AK43 主空道
    double hvAxial_mm = 0.0;           // AA28 高压轴向高
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
    double hvCopperLoss_W = 0.0;       // Y20 高压电阻损耗
    double lvCopperLoss_W = 0.0;       // AH20 低压电阻损耗
    double hvExtraLossPct = 0.0;       // AA45 高压附加损耗 %
    double hvExtraLoss_W = 0.0;        // AC45 高压附加损耗 W
    double loadLoss_W = 0.0;           // L10 负载损耗
    // 导线重
    double hvWireWeight_kg = 0.0;      // Z21 高压导线重
    double lvWireWeight_kg = 0.0;      // AH21 低压导线重
    double wireWeightTotal_kg = 0.0;   // C10 导线总重
};

// ---- 阻抗电压 ----
struct ImpedanceResult {
    double lambda_mm = 0.0;            // M39 漏磁通道总厚 λ
    double hx_mm = 0.0;                // Q30/Q33 绕组电抗高
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

struct CalcResult {
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
