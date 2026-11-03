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
    CoreType coreType = StackedSilicon;

    // 铁芯截面形状
    enum CoreShape { Circle, LongRound, Ellipse, HalfEllipse, EllipseLike };
    CoreShape coreShape = Ellipse;

    // 绕组方式
    enum WindingForm { Dual, DualSplit };
    WindingForm windingForm = Dual;

    // 高压线圈结构
    enum HvCoilStructure { MultiLayerCylinder, TwoSegCylinder };
    // TwoSegCylinder保留原枚举值；原表分段圆筒式（两段串联），尚未开放计算。
    HvCoilStructure hvCoilStructure = MultiLayerCylinder;
};

#endif // STRUCTURECONFIG_H
