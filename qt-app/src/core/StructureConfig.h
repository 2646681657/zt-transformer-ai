#ifndef STRUCTURECONFIG_H
#define STRUCTURECONFIG_H
// 变压器结构配置枚举（铁芯类型/绕组方式/线圈结构等选型组合）

#include <QString>

struct StructureConfig {
    // 变压器大类
    enum TransformerCategory { OilImmersed, DryType };
    TransformerCategory category = OilImmersed;

    // 绕组工艺
    enum WindingProcess { FoilWound, WireWound };
    WindingProcess windingProcess = FoilWound;

    // 计算模式
    enum CalcMode { Normal, Professional };
    CalcMode calcMode = Normal;

    // 变压器结构（铁芯类型）
    enum CoreType { StackedSilicon, StereoscopicRoll, PlanarAmorphous };
    CoreType coreType = PlanarAmorphous;

    // 铁芯截面形状
    enum CoreShape { Circle, LongRound, Ellipse, HalfEllipse, EllipseLike };
    CoreShape coreShape = EllipseLike;

    // 绕组方式
    enum WindingForm { Dual, DualSplit };
    WindingForm windingForm = Dual;

    // 高压线圈结构
    enum HvCoilStructure { MultiLayerCylinder, TwoSegCylinder };
    HvCoilStructure hvCoilStructure = MultiLayerCylinder;
};

#endif // STRUCTURECONFIG_H
