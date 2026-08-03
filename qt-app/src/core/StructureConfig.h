#ifndef STRUCTURECONFIG_H
#define STRUCTURECONFIG_H

#include <QString>

struct StructureConfig {
    enum TransformerType { Distribution, Power };
    TransformerType transformerType = Distribution;

    enum CoreShape { Circle, LongRound, Ellipse, HalfEllipse, EllipseLike };
    CoreShape coreShape = EllipseLike;

    enum YokeType { DShape, Flat };
    YokeType yokeType = DShape;
    double yokeScaleFactor = 1.0;
    double columnScaleFactor = 1.0;
    double effectiveAreaFactor = 0.97;

    enum WindingForm { Dual, DualSplit };
    WindingForm windingForm = Dual;

    enum LvCoilStructure { FoilWound, LayerWound };
    LvCoilStructure lvCoilStructure = FoilWound;

    enum HvCoilStructure { MultiLayerCylinder, TwoSegCylinder };
    HvCoilStructure hvCoilStructure = MultiLayerCylinder;

    enum CoilMaterial { Copper, Aluminum };
    CoilMaterial lvMaterial = Copper;
    CoilMaterial hvMaterial = Copper;
    QString insulationType = QStringLiteral("标准绝缘");

    bool hasOilConservator = true;
    bool usePressPackScheme = false;
    bool useVacuumOilFill = true;
    bool useDryingProcess = true;
    bool useHeatShrinkProcess = false;

    QString stereoCalcMethod = QStringLiteral("按叠片");
    double stackingFactor = 0.97;
    double additionalLossFactor = 1.0;
    double strayLossFactor = 1.0;
    QString tempRiseCalcMethod = QStringLiteral("标准计算");
    QString impedanceCalcMethod = QStringLiteral("标准计算");
};

#endif // STRUCTURECONFIG_H
