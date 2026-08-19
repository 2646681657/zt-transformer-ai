#ifndef OPTIMIZATIONRESULT_H
#define OPTIMIZATIONRESULT_H
// 优化方案结果（单个方案的成本/铁芯尺寸/绕组参数）

#include "CalcInput.h"
#include "CalcResult.h"

struct OptimizationResult {
    int schemeIdx = 0;
    double costCuFeOil = 0.0;
    double costCuFe = 0.0;
    double coreD = 0.0;
    double coreL = 0.0;
    int lvTurns = 0;
    double lvRuleT = 0.0;
    double lvRuleW = 0.0;
    double hvRuleT = 0.0;
    double hvRuleW = 0.0;
    int hvLayers = 0;
    int lvOilDucts = 0;
    int hvOilDucts = 0;
    double lvToYoke = 0.0;
    double mainDuct = 0.0;
    int lvHalfOilDucts = 0;
    int hvHalfOilDucts = 0;
    double lvHalfDist = 0.0;
};

// 电磁计算输入/输出 → 方案表行数据（快速计算与寻优共用映射）
inline OptimizationResult makeScheme(int schemeIdx, const CalcInput &input,
                                     const CalcResult &result)
{
    OptimizationResult s;
    s.schemeIdx = schemeIdx;
    s.costCuFeOil = result.cost.materialCost;
    s.costCuFe = result.cost.steelCost + result.cost.hvWireCost
                 + result.cost.lvWireCost;
    s.coreD = input.coreDiameter_mm;
    s.coreL = result.core.minorAxis_mm;
    s.lvTurns = input.lvTurns;
    s.lvRuleT = input.lvFoilThick_mm;
    s.lvRuleW = input.lvFoilWidth_mm;
    s.hvRuleT = input.hvBareThick_mm;
    s.hvRuleW = input.hvBareWidth_mm;
    s.hvLayers = result.winding.layerCount;
    s.lvOilDucts = 5;
    s.hvOilDucts = 5;
    s.lvToYoke = input.lvEndInsul_mm;
    s.mainDuct = result.winding.mainDuct_mm;
    return s;
}

#endif // OPTIMIZATIONRESULT_H
