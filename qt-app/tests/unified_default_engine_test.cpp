#include "ElectromagneticEngine.h"
#include "GridSearchSpace.h"
#include "SchemeStore.h"
#include <QCoreApplication>
#include <QFile>
#include <cmath>
#include <cstdio>
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv); ElectromagneticEngine engine;
 const QStringList keys={"coreWidthOffsets_mm","coreSheetCountAdjust","designAreaMode","unitIronLossMode","manualCoreLossPerKg_W","manualYokeLossPerKg_W","centerDistanceAllowance_mm","hvLayerInsulCalcFactor","hvMaxTurnsAdjust","hvAxialAllowance_mm","hvDuctLayerRule","impedanceOutputMode","impedanceCalibrationFactor","activePartHvWeightMode","thermalFormulaMode","lvThermalExtraMode","tankRimHighCapacity_mm","tankWallThicknessOverride_mm","tankBottomThicknessAdjust_mm","waveLongCountAdjust","oilTopCorrection_K"};
 int failures=0; auto check=[&](bool yes,const char *msg){if(!yes){std::fprintf(stderr,"FAIL %s\n",msg);++failures;}};
 auto equal=[&](const CalcResult&a,const CalcResult&b){return a.core.coreArea_cm2==b.core.coreArea_cm2&&a.core.coreWeight_kg==b.core.coreWeight_kg&&a.core.noLoadLoss_W==b.core.noLoadLoss_W&&a.winding.loadLoss_W==b.winding.loadLoss_W&&a.impedance.impedance_pct==b.impedance.impedance_pct&&a.thermal.oilTopRise_K==b.thermal.oilTopRise_K&&a.cost.materialCost==b.cost.materialCost;};
 for(const auto &file:{QString(),QString(":/templates/startup_schemes.json")}) {
  QJsonArray cases; if(file.isEmpty())cases.append(QJsonObject{{"input",SchemeStore::toJson(CalcInput{})}});else{QFile f(file);check(f.open(QIODevice::ReadOnly),"open presets");cases=QJsonDocument::fromJson(f.readAll()).array();}
  for(const auto &v:cases){auto original=v.toObject()["input"].toObject();auto clean=original;for(const auto &k:keys)clean.remove(k);
   CalcResult expected;check(engine.calcElectromagnetic(SchemeStore::fromJson(clean),expected,true),"default-rule case calculates");
   if(file.isEmpty())check(expected.core.coreWeight_kg==905&&expected.core.noLoadLoss_W==524&&expected.winding.loadLoss_W==5206&&expected.impedance.impedance_pct==6.93&&expected.cost.materialCost==62856,"original built-in default numeric regression");
   for(const auto &k:keys){auto one=clean;one[k]=9999;CalcResult n;check(engine.calcElectromagnetic(SchemeStore::fromJson(one),n)&&equal(expected,n),"individual numeric override ignored");}
   auto dirty=original;for(const auto &k:keys)dirty[k]=QJsonValue("obsolete garbage");
   CalcResult actual;const auto loaded=SchemeStore::fromJson(dirty);check(engine.calcElectromagnetic(loaded,actual,true),"obsolete malformed overrides ignored");check(equal(expected,actual),"old overrides cannot affect calculations");
   auto saved=SchemeStore::toJson(loaded);for(const auto &k:keys)check(!saved.contains(k),"deprecated tuning not persisted");
   auto changed=SchemeStore::fromJson(clean);changed.lvFoilThick_mm+=0.025;CalcResult next;check(engine.calcElectromagnetic(changed,next),"real design input calculates");check(!equal(expected,next),"real design input remains effective");
   std::printf("baseline %.2f kVA: P0 %.2f Pk %.2f Z %.2f cost %.2f\n",loaded.capacity_kVA,expected.core.noLoadLoss_W,expected.winding.loadLoss_W,expected.impedance.impedance_pct,expected.cost.materialCost);
  }
 }
 CalcInput base;
 base.hvLayerInsul_mm = 0.09;
 const OptimizationSettings settings;
 const GridSearchSpace space(base, settings);
 const CalcInput candidate = space.inputFor(GridSearchSpace::Point{});
 CalcInput savedEnterprise = base;
 savedEnterprise.useEnterpriseLayerInsulationLimit = true;
 const CalcInput reopened = SchemeStore::fromJson(SchemeStore::toJson(savedEnterprise));
 const GridSearchSpace restartedSpace(reopened, settings);
 const CalcInput restartedCandidate = restartedSpace.inputFor(GridSearchSpace::Point{});
 CalcResult directResult, candidateResult, reopenedResult, restartedResult;
 check(engine.calcElectromagnetic(base, directResult, true)
           && engine.calcElectromagnetic(candidate, candidateResult, true)
           && engine.calcElectromagnetic(reopened, reopenedResult, true)
           && engine.calcElectromagnetic(restartedCandidate, restartedResult, true),
       "insulation limit boundary cases calculate");
 check(directResult.calculationSheetDetails.value(QStringLiteral("X42")).first == 4,
       "direct calculation sizes insulation at 4900 V/mm");
 check(candidateResult.calculationSheetDetails.value(QStringLiteral("X42")).first == 4
           && restartedResult.calculationSheetDetails.value(QStringLiteral("X42")).first == 4,
       "new optimization sizes insulation at 4900 V/mm even from a 5300 V/mm scheme");
 check(reopenedResult.calculationSheetDetails.value(QStringLiteral("X42")).first == 3,
       "reopening a previously saved 5300 V/mm scheme preserves its insulation count");
 check(OilTransformerSearchRules::inputFailures(candidate).isEmpty()
           && OilTransformerSearchRules::resultFailures(candidate, candidateResult).isEmpty(),
       "default candidate meets the implemented enterprise rules");
 CalcInput foilWidthCase = candidate;
 foilWidthCase.lvConnectionBarWidthText = QStringLiteral("50");
 for (double boundary : {150.0, 1200.0}) {
     foilWidthCase.lvFoilWidth_mm = boundary;
     check(OilTransformerSearchRules::inputFailures(foilWidthCase).isEmpty(),
           "copper foil width endpoints are inclusive and independent of the connection bar");
 }
 foilWidthCase.lvFoilWidth_mm = 149.0;
 check(!OilTransformerSearchRules::inputFailures(foilWidthCase).isEmpty(),
       "copper foil below the enterprise minimum is rejected");
 CalcResult magneticBoundary = candidateResult;
 magneticBoundary.core.fluxDensity_core_T = 1.75;
 check(!OilTransformerSearchRules::resultFailures(candidate, magneticBoundary).isEmpty(),
       "the 1.75 T magnetic density ceiling is strict");
 CalcInput steelCase = base;
 steelCase.steelGrade = QStringLiteral("23R080");
 const GridSearchSpace steelSpace(steelCase, settings);
 check(steelSpace.inputFor(GridSearchSpace::Point{}).stackFactor == 0.96,
       "optimization assigns the stacking factor for 0.23 mm steel");
 return failures?1:0;
}
