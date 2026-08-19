#ifndef TRANSFORMERPARAMS_H
#define TRANSFORMERPARAMS_H
// 变压器输入参数结构体（容量/电压/损耗/温升等设计输入）

#include <QString>

struct TransformerParams {
    // 输入信息（默认值对齐 SB20-M-630-10 计算单）
    double capacity_kVA = 630.0;
    double hvRatedVoltage_kV = 10.0;
    double lvRatedVoltage_kV = 0.4;
    int hvTapStages = 4;
    double hvTapVoltagePercent = 2.5;
    double maxAmbientTemp_C = 40.0;
    double maxAltitude_m = 1000.0;

    // 性能指标（标准值取自 SB20-M-630-10 计算单基准：空载 529W、
    // 负载 5206W、总损 5735W、阻抗 6.93%、空载电流 0.6%）
    double noLoadLossStd_W = 530.0;
    double noLoadLossMaxDev_pct = 0.0;
    double loadLossStd_W = 5210.0;
    double loadLossMaxDev_pct = 0.0;
    double totalLossStd_W = 5740.0;
    double totalLossMaxDev_pct = 0.0;
    double impedanceVoltageStd_pct = 6.93;
    double impedanceVoltageMaxDev_pct = 10.0;
    double impedanceVoltageMinDev_pct = -10.0;
    double noLoadCurrentStd_pct = 0.6;
    double noLoadCurrentMaxDev_pct = 30.0;
    double oilTopTempRise_K = 60.0;
    double hvCoilTempRise_K = 65.0;
    double lvCoilTempRise_K = 65.0;

    // 选项
    QString efficiencyCalcMethod = QStringLiteral("标准计算");
    QString productModel = QStringLiteral("S13配变");
    QString connectionGroup = QStringLiteral("Dyn");
    int frequency_Hz = 50;
    QString environmentGrade = QStringLiteral("C3");
    double calcRefTemp_C = 75.0;
    QString coreSectionCalcMethod = QStringLiteral("自动计算");
    QString loadLossCalcMethod = QStringLiteral("电阻+涡流+杂散");
};

#endif // TRANSFORMERPARAMS_H
