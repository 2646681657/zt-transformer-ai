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
    // Values are persisted in scheme JSON; do not reorder or reuse them.
    enum CoreShape { UnknownCoreShape = -1, Circle = 0, LongRound = 1, Ellipse = 2,
                     HalfEllipse = 3, EllipseLike = 4 };
    CoreShape coreShape = Ellipse;

    enum YokeShape { UnknownYokeShape = -1, SameAsCore = 0, TYoke = 1 };
    YokeShape yokeShape = TYoke;

    static QString coreShapeName(CoreShape shape) {
        switch (shape) {
        case Circle: return QStringLiteral("圆形");
        case LongRound: return QStringLiteral("长圆形");
        case Ellipse: return QStringLiteral("椭圆形");
        case HalfEllipse: return QStringLiteral("半椭圆形");
        case EllipseLike: return QStringLiteral("类椭圆形");
        default: return QStringLiteral("未识别心柱");
        }
    }
    static QString yokeShapeName(YokeShape shape) {
        switch (shape) {
        case SameAsCore: return QStringLiteral("同心柱");
        case TYoke: return QStringLiteral("T形轭");
        default: return QStringLiteral("未识别铁轭");
        }
    }

    // 绕组方式
    enum WindingForm { Dual, DualSplit };
    WindingForm windingForm = Dual;

    // 高压线圈结构
    enum HvCoilStructure { MultiLayerCylinder, TwoSegCylinder };
    // TwoSegCylinder保留原枚举值；原表分段圆筒式（两段串联），尚未开放计算。
    HvCoilStructure hvCoilStructure = MultiLayerCylinder;
};

#endif // STRUCTURECONFIG_H
