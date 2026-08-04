#ifndef TRANSFORMERPARAMS_H
#define TRANSFORMERPARAMS_H
// 变压器输入参数结构体（容量/电压/损耗/温升等设计输入）

#include <QString>

struct TransformerParams {
    // 输入信息
    double capacity_kVA = 1600.0;
    double hvRatedVoltage_kV = 6.3;
    double lvRatedVoltage_kV = 0.8;
    int hvTapStages = 2;
    double hvTapVoltagePercent = 2.5;
    double maxAmbientTemp_C = 40.0;
    double maxAltitude_m = 1000.0;

    // 性能指标
    double noLoadLossStd_W = 1170.0;
    double noLoadLossMaxDev_pct = 0.0;
    double loadLossStd_W = 14500.0;
    double loadLossMaxDev_pct = 0.0;
    double totalLossStd_W = 15670.0;
    double totalLossMaxDev_pct = 0.0;
    double impedanceVoltageStd_pct = 4.5;
    double impedanceVoltageMaxDev_pct = 10.0;
    double impedanceVoltageMinDev_pct = -10.0;
    double noLoadCurrentStd_pct = 0.4;
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
