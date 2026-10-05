#ifndef CALCULATIONAPPLICABILITY_H
#define CALCULATIONAPPLICABILITY_H

#include "StructureConfig.h"
#include "TransformerParams.h"
#include "CalcInput.h"
#include <QStringList>
#include <cmath>

// 界面入口共享的支持范围检查；不更改引擎公式或推测新算法。
inline QString calculationScopeError(const StructureConfig &config,
                                     const TransformerParams &params,
                                     const CalcInput &input)
{
    QStringList reasons;
    const QString ductError = input.oilDuctInputError();
    if (!ductError.isEmpty())
        reasons << ductError;
    const QString wireError = input.highVoltageWireError();
    if (!wireError.isEmpty())
        reasons << wireError;
    if (config.category != StructureConfig::OilImmersed)
        reasons << QStringLiteral("干式变压器尚无对应温升算法");
    if (config.windingProcess != StructureConfig::FoilWound)
        reasons << QStringLiteral("当前低压仅支持箔绕，线绕尚未接入");
    if (config.coreType != StructureConfig::StackedSilicon)
        reasons << QStringLiteral("卷铁芯、非晶结构尚未接入对应算法");
    if (config.coreShape != StructureConfig::Ellipse)
        reasons << QStringLiteral("铁芯截面仅支持椭圆形");
    if (config.windingForm != StructureConfig::Dual)
        reasons << QStringLiteral("双分裂绕组尚未接入对应算法");
    if (config.hvCoilStructure != StructureConfig::MultiLayerCylinder || input.hvCoilFormIdx != 1)
        reasons << QStringLiteral("两段及其他高压线圈暂不开放计算：型式对应与参考算例尚待核对");
    if (params.frequency_Hz != 50)
        reasons << QStringLiteral("当前公式固定按50Hz计算，不支持其他频率");
    if (!std::isfinite(params.calcRefTemp_C) || std::abs(params.calcRefTemp_C - 75.0) > 1e-9)
        reasons << QStringLiteral("当前电阻固定折算到75℃，不支持其他折算温度");
    return reasons.join(QStringLiteral("；"));
}

#endif
