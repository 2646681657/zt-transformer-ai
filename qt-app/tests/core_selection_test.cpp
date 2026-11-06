#include "SchemeStore.h"
#include "DesignDatabase.h"
#include "CoreLaminationPreview.h"
#include "CalculationApplicability.h"
#include "SchemeConstraints.h"
#include "ElectromagneticEngine.h"
#include "ParamTableWidget.h"
#include "SchemeCalculationSheet.h"
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QPixmap>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QMessageBox>
#include "ModelStdTable.h"
#include <QFontDatabase>
#include "CalculationSheetTemplate.h"
#include <QJsonDocument>
#include <QSet>
#include <QLabel>
#include <QDialog>
#include <QTabWidget>
#include <QTextBrowser>
#include <QScrollBar>
#include "core_weight_cases.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Offscreen Qt has no bundled fonts; use the same available Windows font
    // for repeatable geometry and legibility checks, with or without capture.
    if (QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc") >= 0)
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"),10));
    int failures = 0;
    const auto check = [&](bool passed, const char *message) {
        if (!passed) { qCritical() << message; ++failures; }
    };
    // A new stored selection must survive a real JSON encode/decode, not fall back.
    for (int shape = 0; shape < 5; ++shape) {
        for (int yoke = 0; yoke < 2; ++yoke) {
            QJsonObject json;
            json.insert("coreShape", shape);
            json.insert("yokeShape", yoke);
            const auto restored = SchemeStore::fromJson(json);
            const auto roundTrip = SchemeStore::toJson(restored);
            check(roundTrip.value("coreShape").isDouble()
                      && roundTrip.value("coreShape").toInt() == shape,
                  "Core shape was lost or replaced during JSON round trip");
            check(roundTrip.value("yokeShape").isDouble()
                      && roundTrip.value("yokeShape").toInt() == yoke,
                  "Yoke shape was lost or replaced during JSON round trip");
        }
    }
    const auto legacy = SchemeStore::toJson(SchemeStore::fromJson({}));
    check(legacy.value("coreShape").toInt(-99) == 2
              && legacy.value("yokeShape").toInt(-99) == 1,
          "Missing legacy fields must migrate to ellipse/T-yoke");

    ElectromagneticEngine engine;
    {
        TransformerParams preset;
        preset.hasNonstandardAn3=true; preset.nonstandardAn3_pct=0.7;
        preset.hasSavedCustomNoLoadCurrent=true; preset.savedCustomNoLoadCurrent_pct=0.36;
        const auto unknown=ParamTableWidget::paramsForInput(preset,CalcInput{});
        check(unknown.standardMode==TransformerParams::StandardMode::Unconfirmed
                  && !unknown.hasNonstandardAn3 && !unknown.hasSavedCustomNoLoadCurrent,
              "Legacy input missing criteria must not inherit another scheme's AN3 or custom cache");
    }
    { // Standards and no-load-current source are independent.
        CalcInput seed;seed.hasPerformanceCriteria=true;seed.performanceCriteria.productModel=QStringLiteral("SB20-M-630/10-0.4");
        seed.performanceCriteria.standardMode=TransformerParams::StandardMode::BuiltIn;seed.performanceCriteria.lossStandardsManual=true;seed.performanceCriteria.noLoadCurrentStd_pct=0.15;
        auto root=SchemeStore::toJson(seed);auto criteria=root.value("performanceCriteria").toObject();criteria.insert("noLoadCurrentSource","manual");root.insert("performanceCriteria",criteria);
        const auto loaded=SchemeStore::fromJson(root);ParamTableWidget editor;editor.loadParamsForConfig(loaded.performanceCriteria,StructureConfig{},loaded,true,false,true);
        check(editor.getParams().standardMode==TransformerParams::StandardMode::BuiltIn,"Manual criterion must not change original built-in standard label");
        check(std::abs(editor.getParams().noLoadCurrentStd_pct-0.15)<1e-9,"Built-in manual current must retain original value");
        check(SchemeStore::toJson(loaded).value("performanceCriteria").toObject().value("noLoadCurrentSource").toString()=="manual","Current source must survive save and reopen");
        auto *source=editor.findChild<QComboBox *>("noLoadCurrentSource");check(source&&source->count()==2,"Current source needs explicit automatic and manual options");
        QComboBox *standard=nullptr;for(auto *c:editor.findChildren<QComboBox *>())if(c->findText(QStringLiteral("国标"))>=0&&c->findText(QStringLiteral("非标"))>=0)standard=c;
        if(source&&standard){
            source->setCurrentIndex(1);standard->setCurrentIndex(standard->findText(QStringLiteral("非标")));
            check(std::abs(editor.getParams().noLoadCurrentStd_pct-0.15)<1e-9,"Manual current must survive switch to custom standard");
            QTimer::singleShot(0,[](){if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))if(auto *yes=box->button(QMessageBox::Yes))yes->click();});
            standard->setCurrentIndex(standard->findText(QStringLiteral("国标")));
            check(editor.getParams().standardMode==TransformerParams::StandardMode::BuiltIn&&std::abs(editor.getParams().noLoadCurrentStd_pct-0.15)<1e-9,"Manual current must survive switch back to built-in standard");
            source->setCurrentIndex(0);check(std::abs(editor.getParams().noLoadCurrentStd_pct-0.56)<1e-9,"Automatic current must use original lookup independently of loss overrides");
        }
        for(const auto bad:{QJsonValue("bad"),QJsonValue(QJsonValue::Null),QJsonValue(1)}){auto invalid=root;auto c=criteria;c.insert("noLoadCurrentSource",bad);invalid.insert("performanceCriteria",c);check(!SchemeStore::fromJson(invalid).hasPerformanceCriteria,"Invalid current source must invalidate saved criteria");}
        {CalcInput manual=loaded,automatic=loaded;manual.performanceCriteria.noLoadCurrentSource=TransformerParams::NoLoadCurrentSource::Manual;automatic.performanceCriteria.noLoadCurrentSource=TransformerParams::NoLoadCurrentSource::Automatic;automatic.performanceCriteria.noLoadCurrentStd_pct=0.56;CalcResult a,b;
        check(engine.calcElectromagnetic(manual,a,true)&&engine.calcElectromagnetic(automatic,b,true)&&a.cost.materialCost==b.cost.materialCost&&a.core.noLoadCurrent_pct==b.core.noLoadCurrent_pct&&a.core.noLoadLoss_W==b.core.noLoadLoss_W&&a.winding.loadLoss_W==b.winding.loadLoss_W&&a.thermal.oilTopRise_K==b.thermal.oilTopRise_K,"Changing current criterion source must not alter physics or price");
        check(a.calculationSheetDetails.value("O11").first!=b.calculationSheetDetails.value("O11").first,"Changing current standard must update deviation");
        check(checkSchemeConstraints(manual.performanceCriteria,a).failedChecks.contains(QStringLiteral("空载电流%"))&&checkSchemeConstraints(automatic.performanceCriteria,b).passed,"Current criterion source must affect the shared optimization constraint without altering physics");}
        {auto j=root;auto c=criteria;c.insert("noLoadCurrentSource","automatic");c.insert("mode","custom");j.insert("performanceCriteria",c);check(!SchemeStore::fromJson(j).hasPerformanceCriteria,"Explicit custom automatic source without AN3 must not accept stale criteria");}
        auto legacy=root;auto c=criteria;c.remove("noLoadCurrentSource");c.insert("mode","custom");legacy.insert("performanceCriteria",c);
        check(SchemeStore::toJson(SchemeStore::fromJson(legacy)).value("performanceCriteria").toObject().value("noLoadCurrentSource").toString()=="manual","Legacy custom final criterion must migrate to manual without subtracting again");
    }

    {
        const QStringList obsoleteKeys={
            QStringLiteral("coreWidthOffsets_mm"),
            QStringLiteral("coreSheetCountAdjust"),
            QStringLiteral("designAreaMode"),
            QStringLiteral("unitIronLossMode"),
            QStringLiteral("manualCoreLossPerKg_W"),
            QStringLiteral("manualYokeLossPerKg_W"),
            QStringLiteral("centerDistanceAllowance_mm"),
            QStringLiteral("hvLayerInsulCalcFactor"),
            QStringLiteral("hvMaxTurnsAdjust"),
            QStringLiteral("hvAxialAllowance_mm"),
            QStringLiteral("hvDuctLayerRule"),
            QStringLiteral("impedanceOutputMode"),
            QStringLiteral("impedanceCalibrationFactor"),
            QStringLiteral("activePartHvWeightMode"),
            QStringLiteral("thermalFormulaMode"),
            QStringLiteral("lvThermalExtraMode"),
            QStringLiteral("tankRimHighCapacity_mm"),
            QStringLiteral("tankWallThicknessOverride_mm"),
            QStringLiteral("tankBottomThicknessAdjust_mm"),
            QStringLiteral("waveLongCountAdjust"),
            QStringLiteral("oilTopCorrection_K")
        };
        // The source workbooks remain historical evidence; unified defaults no
        // longer promise to reproduce their per-workbook tuning choices.
        const auto cases=QJsonDocument::fromJson(coreWeightCasesJson).array();
        check(cases.size()==3,"Historical fixtures must retain all three source worksheets");
        for(const auto &entry:cases){
            const auto fixture=entry.toObject();
            const auto legacyJson=fixture.value("input").toObject();
            auto cleanJson=legacyJson;
            for(const auto &key:obsoleteKeys)cleanJson.remove(key);
            const auto input=SchemeStore::fromJson(legacyJson);
            const auto saved=SchemeStore::toJson(input);
            check(saved==SchemeStore::toJson(SchemeStore::fromJson(cleanJson)),
                  "Legacy tuning must not change the normalized real inputs");
            for(const auto &key:obsoleteKeys)
                check(!saved.contains(key),"Saving historical inputs must omit obsolete tuning");
            CalcResult r,clean,fast;
            const bool ok=engine.calcElectromagnetic(input,r,true);
            check(ok&&engine.calcElectromagnetic(SchemeStore::fromJson(cleanJson),clean,true),
                  "Historical real inputs must calculate with unified defaults");
            if(!ok)continue;
            check(r.calculationSheetDetails==clean.calculationSheetDetails
                      &&r.cost.materialCost==clean.cost.materialCost,
                  "Legacy tuning must not affect detailed calculation results");
            check(engine.calcElectromagnetic(input,fast,false)
                      &&fast.core.coreWeight_kg==r.core.coreWeight_kg
                      &&fast.cost.materialCost==r.cost.materialCost
                      &&fast.core.coreArea_cm2==r.core.coreArea_cm2
                      &&fast.core.magCapacity_vaPerKg==r.core.magCapacity_vaPerKg
                      &&fast.core.noLoadCurrent_pct==r.core.noLoadCurrent_pct
                      &&fast.core.noLoadLoss_W==r.core.noLoadLoss_W
                      &&fast.winding.loadLoss_W==r.winding.loadLoss_W
                      &&fast.impedance.impedance_pct==r.impedance.impedance_pct
                      &&fast.thermal.oilTopRise_K==r.thermal.oilTopRise_K
                      &&fast.thermal.hvWindingRise_K==r.thermal.hvWindingRise_K
                      &&fast.thermal.lvWindingRise_K==r.thermal.lvWindingRise_K
                      &&!fast.calculationSheetDetails.contains("CoreYokeWeight1"),
                  "Optimization and detail paths must use identical unified physics");
            check(input.hasPerformanceCriteria,"Historical standard-source snapshots must remain valid");
            check(saved.value("performanceCriteria").toObject().value("noLoadCurrentSource")
                      ==legacyJson.value("performanceCriteria").toObject().value("noLoadCurrentSource"),
                  "Removing tuning must preserve the actual criterion source");
            ParamTableWidget editor;
            editor.loadParamsForConfig(TransformerParams{},StructureConfig{},input,true,true);
            int conservatorRow=-1;
            for(int row=0;row<editor.rowCount();++row)
                if(editor.item(row,1)&&editor.item(row,1)->text()==QStringLiteral("储油柜重(kg)"))conservatorRow=row;
            check(conservatorRow>=0,"Conservator weight must remain an accessible real input");
            if(conservatorRow>=0){
                editor.item(conservatorRow,2)->setText(QString::number(input.conservatorWeight_kg+10));
                CalcInput edited;editor.saveToInput(edited);
                check(edited.conservatorWeight_kg==input.conservatorWeight_kg+10,
                      "Conservator weight edits must reach the saved input");
            }
            CalcInput ordinary;
            editor.loadParamsForConfig(TransformerParams{},StructureConfig{},input,false,true);
            editor.saveToInput(ordinary);
            check(ordinary.conservatorWeight_kg==input.conservatorWeight_kg,
                  "Ordinary mode must retain conservator weight");
            auto changedJson=saved;changedJson.insert("conservatorWeight_kg",input.conservatorWeight_kg+10);
            CalcResult changed;
            check(engine.calcElectromagnetic(SchemeStore::fromJson(changedJson),changed,true)
                      &&changed.mass.tankWeight_kg==r.mass.tankWeight_kg+10
                      &&changed.mass.oilWeight_kg==r.mass.oilWeight_kg
                      &&changed.core.coreWeight_kg==r.core.coreWeight_kg,
                  "Conservator weight must add tank mass without changing core or oil displacement");
            changedJson=saved;changedJson.insert("lvEndInsul_mm",input.lvEndInsul_mm+1);
            check(engine.calcElectromagnetic(SchemeStore::fromJson(changedJson),changed,true)
                      &&changed.mass.windowHeight_mm==r.mass.windowHeight_mm+2
                      &&changed.mass.centerDistance_mm==r.mass.centerDistance_mm
                      &&changed.core.coreLegsWeight_kg>r.core.coreLegsWeight_kg
                      &&changed.core.yokesWeight_kg==r.core.yokesWeight_kg,
                  "Real window inputs must recalculate leg mass without changing yoke mass");
        }
        for(const auto bad:{QJsonValue("bad"),QJsonValue(QJsonValue::Null)}){
            QJsonObject invalid;invalid.insert("conservatorWeight_kg",bad);CalcResult r;
            check(!engine.calcElectromagnetic(SchemeStore::fromJson(invalid),r),
                  "Malformed real conservator weight must still be rejected");
        }
    }
    CalcResult baseline;
    check(engine.calcElectromagnetic(CalcInput{}, baseline) && baseline.valid,
          "Default ellipse/T-yoke baseline must remain calculable");
    check(std::abs(baseline.cost.materialCost - 62856.0) < 0.00001
              && baseline.core.noLoadLoss_W == 524.0 && baseline.winding.loadLoss_W == 5206.0
              && baseline.impedance.impedance_pct == 6.93 && baseline.thermal.oilTopRise_K == 38.0,
          "Default cost must follow worksheet line rounding while electrical results remain unchanged");
    // Original cost sheets store each amount at integer-yuan precision before summing.
    // Test the actual engine, including custom fractional prices and steel price.
    for (const bool customPrice : {false, true}) {
        CalcInput input;
        if (customPrice) {
            input.materialPrices.custom = true;
            input.materialPrices.copper = 60.13;
            input.materialPrices.oil = 10.17;
            input.materialPrices.tank = 9.19;
            input.useCustomSteelPrice = true;
            input.steelPricePerKg = 17.23;
            input.steelPriceGrade = input.steelGrade;
        }
        CalcResult priced;
        check(engine.calcElectromagnetic(input, priced), "Cost precision test must calculate");
        const auto prices = input.materialPrices.effective();
        const double expected[] = {
            std::round(std::round(priced.core.coreWeight_kg * 1.05) * (customPrice ? 17.23 : 17.0)),
            std::round(std::round(priced.winding.hvWireWeight_kg * 1.05) * (prices.copper + 3.3)),
            std::round(std::round(priced.winding.lvWireWeight_kg * 1.08) * (prices.copper * 1.05 + 6.5)),
            std::round(std::round(priced.mass.oilWeight_kg * 1.1) * prices.oil),
            std::round((std::round(priced.mass.tankWeight_kg * 1.05) + 200.0) * prices.tank)
        };
        const double actual[] = {priced.cost.steelCost, priced.cost.hvWireCost,
            priced.cost.lvWireCost, priced.cost.oilCost, priced.cost.tankCost};
        double expectedTotal = 0.0;
        for (int j = 0; j < 5; ++j) {
            check(std::abs(actual[j] - expected[j]) < 1e-8,
                  "Material cost must use worksheet integer-yuan line items");
            expectedTotal += expected[j];
        }
        check(std::abs(priced.cost.materialCost - expectedTotal) < 1e-8,
              "Total material cost must sum already rounded line items");
        check(priced.winding.loadLoss_W == baseline.winding.loadLoss_W
                  && priced.core.noLoadLoss_W == baseline.core.noLoadLoss_W
                  && priced.mass.totalWeight_kg == baseline.mass.totalWeight_kg,
              "Price precision must not change loss or mass calculations");
    }
    qInfo().noquote() << QStringLiteral("Baseline: cost=%1 noLoad=%2 load=%3 impedance=%4 oilTop=%5")
        .arg(baseline.cost.materialCost, 0, 'g', 16).arg(baseline.core.noLoadLoss_W, 0, 'g', 16)
        .arg(baseline.winding.loadLoss_W, 0, 'g', 16).arg(baseline.impedance.impedance_pct, 0, 'g', 16)
        .arg(baseline.thermal.oilTopRise_K, 0, 'g', 16);
    {
        // Keep the unaffected default geometry as an independent regression.
        CoreResult geometry;QString error;
        const bool ok=ElectromagneticEngine::previewCoreGeometry(CalcInput{},geometry,error);
        check(ok,"Default core geometry must remain available");
        const double widths[]={180,175,170,165,160,150,140,130,120,110,100};
        const double stacks[]={36,15,11,9,9,12,8,6,6,4,4};
        if(ok)for(int i=0;i<11;++i){
            check(geometry.widths_mm[i]==widths[i],"Default stage widths must retain their independent baseline");
            check(geometry.stacks_mm[i]==stacks[i],"Default half-stacks must retain their independent baseline");
        }
        const auto widthJson=SchemeStore::toJson(CalcInput{});
        for(const auto change:{qMakePair(QString("coreDiameter_mm"),175.0),qMakePair(QString("coreStraight_mm"),110.0),qMakePair(QString("ellipseAngle_deg"),20.0)}){
            auto json=widthJson;json.insert(change.first,change.second);const auto input=SchemeStore::fromJson(json);
            CoreResult geometry;QString error;const bool ok=ElectromagneticEngine::previewCoreGeometry(input,geometry,error);
            check(ok,"Width rule must remain dynamic after diameter straight or angle changes");if(!ok)continue;
            QVector<double> bases;check(DesignDatabase::instance().coreWidths(geometry.minorAxis_mm,bases),"Read independently matched lookup row");
            const double radians=input.ellipseAngle_deg*std::acos(-1.0)/180;
            const double radius=std::round((input.coreDiameter_mm/2+input.coreStraight_mm/2/std::sin(radians))*100)/100;
            const double flat=std::round(input.coreStraight_mm/2/std::tan(radians)),junction=std::round(radius*std::sin(radians));
            double cumulative=0;
            for(int i=0;i<11&&i<bases.size();++i){
                const double width=bases[i]+CalcInput::coreWidthOffsets_mm[i];
                check(geometry.widths_mm[i]==width,"Geometry changes must rerun lookup plus fixed default offsets");
                const double arc=std::sqrt(radius*radius-std::pow(width/2+flat,2));
                const double candidate=(i==0||arc<=junction)?arc:std::sqrt(std::pow(input.coreDiameter_mm/2,2)-std::pow(width/2,2))+input.coreStraight_mm/2;
                const double stack=std::round(candidate-cumulative);cumulative+=stack;
                check(geometry.stacks_mm[i]==stack,"Geometric half-stacks must follow original recursive formula after driver changes");
            }
        }
        const auto circle=CoreLaminationPreview::calculate(StructureConfig::Circle,170,0,DesignDatabase::instance().coreRows());
        QVector<double> circleBases;DesignDatabase::instance().coreWidths(170,circleBases);
        check(circle.available && circle.stages[1].width_mm==circleBases[1]+CalcInput::coreWidthOffsets_mm[1],
              "Independent circle preview must use the same default width offsets");
        auto reservedJson=SchemeStore::toJson(CalcInput{});QJsonArray reservedSteps;
        for(int j=0;j<20;++j)reservedSteps.append(j==16?12.5:CalcInput{}.yokeSteps_mm[j]);
        reservedJson.insert("yokeSteps_mm",reservedSteps);
        const auto reservedInput=SchemeStore::fromJson(reservedJson);
        check(SchemeStore::toJson(reservedInput).value("yokeSteps_mm").toArray()==reservedSteps,
              "Twenty-level real yoke steps must survive scheme round trip");
        ParamTableWidget reserveEditor;
        reserveEditor.loadParamsForConfig(TransformerParams{},StructureConfig{},reservedInput,true,true);
        int reserveRow=-1;
        for(int row=0;row<reserveEditor.rowCount();++row)
            if(reserveEditor.item(row,1)&&reserveEditor.item(row,1)->text()==QStringLiteral("轭阶梯 20 外伸半宽(mm)"))reserveRow=row;
        check(reserveRow>=0,"Real yoke step inputs must expose twenty levels");
        if(reserveRow>=0){
            reserveEditor.item(reserveRow,2)->setText("8.5");
            CalcInput edited;reserveEditor.saveToInput(edited);
            check(SchemeStore::toJson(edited).value("yokeSteps_mm").toArray().at(19).toDouble()==8.5,
                  "Twentieth-level yoke step edits must reach the saved scheme");
        }
        reserveEditor.loadParamsForConfig(TransformerParams{},StructureConfig{},reservedInput,false,true);
        CalcInput ordinaryReserve;reserveEditor.saveToInput(ordinaryReserve);
        check(SchemeStore::toJson(ordinaryReserve).value("yokeSteps_mm").toArray()==reservedSteps,
              "Ordinary mode must preserve real yoke step reserves");
        CalcResult snapshot,lightweight;
        check(engine.calcElectromagnetic(reservedInput,snapshot,true)
                  &&snapshot.core.coreWeight_kg==baseline.core.coreWeight_kg
                  &&snapshot.cost.materialCost==baseline.cost.materialCost
                  &&snapshot.core.noLoadLoss_W==baseline.core.noLoadLoss_W,
              "Reserved yoke stages without geometry must not change mass loss or price");
        check(engine.calcElectromagnetic(reservedInput,lightweight,false)
                  &&lightweight.cost.materialCost==snapshot.cost.materialCost
                  &&lightweight.core.coreWeight_kg==snapshot.core.coreWeight_kg
                  &&lightweight.core.noLoadLoss_W==snapshot.core.noLoadLoss_W
                  &&!lightweight.calculationSheetDetails.contains(QStringLiteral("CoreSheetCount1")),
              "Optimizer must use the same fixed sheet counts without display snapshots");
        SchemeCalculationSheet popup(TransformerParams{},StructureConfig{},reservedInput,snapshot,true);
        auto *supplement=popup.findChild<QPushButton *>(QStringLiteral("supplementParametersButton"));
        check(supplement,"Unified sheet needs supplement entry");
        if(supplement){
            QTimer::singleShot(0,[&]{
                auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
                check(dialog,"Core detail supplement must open");if(!dialog)return;
                auto *details=dialog->findChild<QTextBrowser *>(QStringLiteral("supplementDetails"));
                check(details && details->toPlainText().contains(QStringLiteral("铁芯逐级片数与实际叠厚"))
                          && details->toPlainText().contains(QStringLiteral("查表基数")),
                      "Supplement must show actual sheet counts from the current calculation");
                if(auto *tabs=dialog->findChild<QTabWidget *>())tabs->setCurrentIndex(1);
                QApplication::processEvents();
                if(details)details->verticalScrollBar()->setValue(0);
                const auto path=qEnvironmentVariable("ZTBLD_CORE_ADJUST_CAPTURE");
                if(!path.isEmpty())check(dialog->grab().save(path),"Capture core adjustment detail table");
                if(auto *details=dialog->findChild<QTextBrowser *>(QStringLiteral("supplementDetails"))){
                    check(details->toPlainText().contains(QStringLiteral("铁芯分项重量核对"))&&details->toPlainText().contains(QStringLiteral("中心距附加量")),"Core weight details must expose stage masses and process settings");
                    if(!path.isEmpty()){details->find(QStringLiteral("铁芯分项重量核对"));details->verticalScrollBar()->setValue(details->verticalScrollBar()->value()+details->viewport()->height()-40);QApplication::processEvents();check(dialog->grab().save(QString(path).replace(".png","-weights.png")),"Capture core weight details");}
                }
                dialog->accept();
            });
            supplement->click();
        }
    }
    {
        auto json=SchemeStore::toJson(CalcInput{});
        json.insert(QStringLiteral("oilTopCorrection_K"),9.0);
        const auto restored=SchemeStore::fromJson(json);CalcResult updated;
        check(!SchemeStore::toJson(restored).contains(QStringLiteral("oilTopCorrection_K")),
              "Saving must omit the obsolete oil top correction");
        check(engine.calcElectromagnetic(restored,updated,true)&&updated.valid
                  &&updated.thermal.oilTopRise_K==baseline.thermal.oilTopRise_K,
              "Legacy oil top correction must not change unified temperatures");
    }
    for (const auto key : {QStringLiteral("coreShape"), QStringLiteral("yokeShape")}) {
        for (const auto malformed : {QJsonValue(QJsonValue::Null), QJsonValue(true),
                 QJsonValue("2"), QJsonValue(0.5), QJsonValue(-1), QJsonValue(99),
                 QJsonValue(QJsonObject{}), QJsonValue(QJsonArray{})}) {
            auto json = SchemeStore::toJson(CalcInput{});
            json.insert(key, malformed);
            const auto input = SchemeStore::fromJson(json);
            CalcResult result;
            check(!engine.calcElectromagnetic(input, result) && !result.valid && !result.error.isEmpty(),
                  "Explicit malformed selection must be rejected by the engine");
            const auto persisted = SchemeStore::fromJson(SchemeStore::toJson(input));
            check(!engine.calcElectromagnetic(persisted, result),
                  "Saving an invalid selection must not repair it into a supported branch");
        }
    }
    for (const auto key : {QStringLiteral("coreShape"), QStringLiteral("yokeShape")}) {
        auto json = SchemeStore::toJson(CalcInput{});
        json.insert(key, 0); // circle or same-as-core yoke: known but unsupported
        const auto input = SchemeStore::fromJson(json);
        CalcResult result;
        check(!engine.calcElectromagnetic(input, result) && !result.valid,
              "Unsupported known selection must not run the ellipse engine");
        CoreResult core;
        QString error;
        check(!ElectromagneticEngine::previewCoreGeometry(input, core, error) && !error.isEmpty(),
              "Unsupported selection must not produce an ellipse geometry preview");
        check(!calculationScopeError(StructureConfig{}, TransformerParams{}, input).isEmpty(),
              "Supported UI configuration must not hide an unsupported saved selection");
    }
    // Hidden input table must collect the current ribbon selection, not a previous input.
    StructureConfig config;
    config.coreShape = StructureConfig::Circle;
    ParamTableWidget table;
    table.loadParamsForConfig(TransformerParams{}, config, CalcInput{}, true, true);
    CalcInput collected;
    table.saveToInput(collected);
    check(SchemeStore::toJson(collected).value("coreShape").toInt(-99) == 0,
          "Ribbon core selection must be transferred into CalcInput");
    config.coreShape = StructureConfig::Ellipse;
    config.yokeShape = StructureConfig::SameAsCore;
    table.setCalculationConfig(config);
    table.saveToInput(collected);
    check(SchemeStore::toJson(collected).value("yokeShape").toInt(-99) == 0,
          "Changing calculation configuration must not discard yoke selection");
    const auto unsupportedPrint = engine.calculate(TransformerParams{}, config);
    check(unsupportedPrint.rows.size() == 1
              && unsupportedPrint.rows.first().leftName == QStringLiteral("电磁计算失败"),
          "Legacy print calculation must not ignore an unsupported yoke");

    auto json = SchemeStore::toJson(CalcInput{});
    json.insert("coreShape", 1);
    json.insert("yokeShape", 0);
    const auto stored = SchemeStore::fromJson(json);
    QTemporaryDir dir;
    const auto path = dir.filePath("schemes.json");
    check(SchemeStore::saveSchemes(path, {stored}), "Could not write scheme fixture");
    bool ok = false;
    const auto loaded = SchemeStore::loadSchemes(path, &ok);
    check(ok && loaded.size() == 1, "Could not reload scheme fixture");
    check(loaded.size() == 1 && SchemeStore::toJson(loaded.first()).value("coreShape").toInt(-99) == 1
              && SchemeStore::toJson(loaded.first()).value("yokeShape").toInt(-99) == 0,
          "File persistence must preserve long-round and same-as-core selections");
    const auto restored = loaded.isEmpty() ? stored : loaded.first();
    SchemeCalculationSheet sheet(TransformerParams{}, StructureConfig{}, restored, baseline, true);
    auto *grid = sheet.findChild<QTableWidget *>();
    // The visible sheet has 38 columns; the hidden parameter table has six.
    for (auto *candidate : sheet.findChildren<QTableWidget *>())
        if (candidate->columnCount() == 38) grid = candidate;
    auto *shape = grid ? qobject_cast<QComboBox *>(grid->cellWidget(3, 5)) : nullptr;
    auto *yoke = grid ? qobject_cast<QComboBox *>(grid->cellWidget(4, 5)) : nullptr;
    check(shape && shape->currentText().contains(QStringLiteral("长圆形")),
          "Popup must display the saved shape, not current page ellipse");
    check(yoke && yoke->currentText().contains(QStringLiteral("同心柱")),
          "Popup must display the saved yoke, not default T-yoke");
    if (shape && yoke) {
        shape->setCurrentIndex(0);
        sheet.inputTable()->saveToInput(collected);
        auto saved = SchemeStore::toJson(collected);
        check(saved.value("coreShape").toInt(-99) == 0
                  && saved.value("yokeShape").toInt(-99) == 0,
              "Popup changes must reach its hidden input table");
        QString error;
        check(!sheet.inputsReady(error), "Unsupported popup selections must still block confirmation");
        shape->setCurrentIndex(2);
        yoke->setCurrentIndex(1);
        sheet.inputTable()->saveToInput(collected);
        saved = SchemeStore::toJson(collected);
        check(sheet.inputsReady(error) && saved.value("coreShape").toInt(-99) == 2
                  && saved.value("yokeShape").toInt(-99) == 1,
              "Switching back must restore the supported selection in collected data");
        check(stored.coreShape == StructureConfig::LongRound && stored.yokeShape == StructureConfig::SameAsCore,
              "Popup edits must not mutate the caller's original scheme");
    }
    // Loading another saved scheme must not inherit the current page's shape.
    config.coreShape = StructureConfig::Circle;
    SchemeCalculationSheet legacySheet(TransformerParams{}, config,
        SchemeStore::fromJson({}), baseline, true);
    CalcInput legacyCollected;
    legacySheet.inputTable()->saveToInput(legacyCollected);
    check(legacyCollected.coreShape == StructureConfig::Ellipse && legacyCollected.yokeShape == StructureConfig::TYoke,
          "A legacy scheme must not inherit the current page's circle selection");
    // Ordinary/professional reloads retain the same selection and geometric inputs.
    table.loadParamsForConfig(TransformerParams{}, config, collected, false, true);
    table.saveToInput(collected);
    table.loadParamsForConfig(TransformerParams{}, config, collected, true, true);
    table.saveToInput(collected);
    check(collected.coreShape == StructureConfig::Circle && collected.yokeShape == StructureConfig::SameAsCore
              && collected.coreStraight_mm == 103.0 && collected.ellipseAngle_deg == 18.0,
          "Mode reload must preserve selection without clearing geometry inputs");
    // Real popup regression: switching shape must replace old ellipse outputs,
    // while unknown whole-machine results remain unavailable, not valid zeros.
    CalcResult detailed;
    check(engine.calcElectromagnetic(CalcInput{}, detailed, true), "Could not prepare detailed fixture");
    QFile applicationTheme(QStringLiteral(":/styles/ztf_theme.qss"));
    check(applicationTheme.open(QIODevice::ReadOnly), "Production theme must be available");
    app.setStyleSheet(QString::fromUtf8(applicationTheme.readAll()));
    {
        TransformerParams criteria;
        criteria.productModel = QStringLiteral("SB20-M-630/10-0.4");
        criteria.standardMode = TransformerParams::StandardMode::Custom;
        criteria.noLoadCurrentStd_pct = 0.6;
        SchemeCalculationSheet customSheet(criteria, StructureConfig{}, CalcInput{}, detailed, true);
        auto *raw = customSheet.findChild<QLineEdit *>("an3RawCurrent");
        check(raw, "Nonstandard raw current editor must exist");
        if (raw) {
            check(raw->text().isEmpty() && std::abs(customSheet.inputTable()->getParams().noLoadCurrentStd_pct-0.6)<1e-9,
                  "Legacy custom criterion must not be treated as AN3 or reduced again");
            auto *currentSource=customSheet.inputTable()->findChild<QComboBox *>("noLoadCurrentSource");check(currentSource,"Raw AN3 scenario requires an explicit source selector");if(currentSource)currentSource->setCurrentIndex(0);
            QTableWidgetItem *waitingCurrent=nullptr;
            for(int row=0;row<customSheet.inputTable()->rowCount();++row)
                if(customSheet.inputTable()->item(row,1) && customSheet.inputTable()->item(row,1)->text()==QStringLiteral("空载电流标准值(%)"))
                    waitingCurrent=customSheet.inputTable()->item(row,2);
            check(waitingCurrent && waitingCurrent->text().isEmpty(),
                  "Automatic custom current missing AN3 must clear stale standard from display");
            QString missingAn3Error;
            check(!customSheet.inputsReady(missingAn3Error),"Missing AN3 must block confirmation");
            raw->setText(QStringLiteral("0.7"));
            check(std::abs(customSheet.inputTable()->getParams().noLoadCurrentStd_pct - 0.36) < 1e-9,
                  "Changing AN3 must automatically update O9");
            QTableWidget *grid = nullptr;
            for (auto *candidate : customSheet.findChildren<QTableWidget *>())
                if (candidate->columnCount() == 38) grid = candidate;
            auto *mode = grid ? qobject_cast<QComboBox *>(grid->cellWidget(2,5)) : nullptr;
            check(mode, "Standard mode selector must be available");
            if (mode) {
                QTimer::singleShot(0, [] {
                    if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                        if (auto *yes=box->button(QMessageBox::Yes)) yes->click();
                        else box->accept();
                    }
                });
                mode->setCurrentIndex(0);
                check(std::abs(customSheet.inputTable()->getParams().noLoadCurrentStd_pct-0.56)<1e-9,
                      "Switching to built-in must use the original 630 kVA table");
                CalcInput builtinSaved; customSheet.inputTable()->saveToInput(builtinSaved);
                const auto reopenedBuiltin=SchemeStore::fromJson(SchemeStore::toJson(builtinSaved));
                check(reopenedBuiltin.hasPerformanceCriteria && reopenedBuiltin.performanceCriteria.standardMode == TransformerParams::StandardMode::BuiltIn
                          && reopenedBuiltin.performanceCriteria.hasNonstandardAn3
                          && std::abs(reopenedBuiltin.performanceCriteria.nonstandardAn3_pct-0.7)<1e-9
                          && std::abs(reopenedBuiltin.performanceCriteria.noLoadCurrentStd_pct-0.56)<1e-9,
                      "Built-in saved scheme must retain custom AN3 without applying it in built-in mode");
                mode->setCurrentIndex(1);
                check(std::abs(customSheet.inputTable()->getParams().noLoadCurrentStd_pct-0.36)<1e-9,
                      "Switching back must restore the nonstandard criterion without double subtraction");
            }
            CalcInput saved; customSheet.inputTable()->saveToInput(saved);
            CalcResult customResult;
            check(engine.calcElectromagnetic(saved, customResult, true)
                      && std::abs(customResult.calculationSheetDetails.value(QStringLiteral("O11")).first-66.67)<1e-6,
                  "Nonstandard O11 must recalculate as 66.67 percent for the sample");
            const auto restored = SchemeStore::fromJson(SchemeStore::toJson(saved));
            check(restored.hasPerformanceCriteria && std::abs(restored.performanceCriteria.noLoadCurrentStd_pct-0.36)<1e-9,
                  "Converted criterion must survive scheme save and reload");
            SchemeCalculationSheet reopened(restored.performanceCriteria, StructureConfig{}, restored, detailed, true);
            auto *reopenedRaw = reopened.findChild<QLineEdit *>("an3RawCurrent");
            check(reopenedRaw && std::abs(reopenedRaw->text().toDouble()-0.7)<1e-9,
                  "Original AN3 value must survive save and reopen");
            raw->setText(QStringLiteral("0.2"));
            QString error;
            check(!customSheet.inputsReady(error), "Invalid AN3 must prevent confirming stale criterion");
        }
    }
    {
        double value=-1.0;
        check(ModelStdTable::originalNoLoadCurrent(630.0, value) && std::abs(value-0.56)<1e-9,
              "Original lookup must match 630 kVA");
        check(!ModelStdTable::originalNoLoadCurrent(640.0, value),
              "Missing capacity must not silently use another capacity");
        TransformerParams unsupported;
        unsupported.capacity_kVA = 640.0;
        unsupported.productModel = QStringLiteral("SB20-M-640/10-0.4");
        ParamTableWidget unknownCapacity;
        unknownCapacity.loadParamsForConfig(unsupported, StructureConfig{}, CalcInput{}, true, false, true);
        check(unknownCapacity.standardStatus().contains(QStringLiteral("未收录")),
              "Unlisted built-in capacity must be marked unconfirmed rather than silently matched");
        TransformerParams collected; CalcInput collectedInput; QString reason;
        check(!unknownCapacity.collectForCalculation(collected, collectedInput, reason, false),
              "Unlisted built-in capacity must block automatic calculation until checked");
    }
    {
        TransformerParams builtin;
        builtin.productModel = QStringLiteral("SB20-M-630/10-0.4");
        builtin.standardMode = TransformerParams::StandardMode::BuiltIn;
        SchemeCalculationSheet sheet(builtin, StructureConfig{}, CalcInput{}, detailed, true);
        check(std::abs(sheet.inputTable()->getParams().noLoadCurrentStd_pct - 0.56) < 1e-9,
              "630 kVA built-in current must match original 0.9 minus 0.34");
        CalcInput saved; sheet.inputTable()->saveToInput(saved);
        CalcResult builtinResult;
        check(engine.calcElectromagnetic(saved, builtinResult, true)
                  && std::abs(builtinResult.calculationSheetDetails.value(QStringLiteral("O11")).first-7.14)<1e-6,
              "Built-in O11 must recalculate as 7.14 percent for the original sample");
    }
    // Performance deviations and constraints are separate from physical calculation.
    {
        TransformerParams criteria;
        criteria.productModel=QStringLiteral("SB20-M-630/10-0.4");
        criteria.standardMode=TransformerParams::StandardMode::Custom;
        criteria.noLoadLossStd_W=510; criteria.loadLossStd_W=4960;
        criteria.noLoadCurrentStd_pct=0.7; criteria.impedanceVoltageStd_pct=6.93;
        SchemeCalculationSheet sample(criteria,StructureConfig{},CalcInput{},detailed,true);
        QTableWidget *grid=nullptr;
        for(auto *candidate:sample.findChildren<QTableWidget *>())if(candidate->columnCount()==38)grid=candidate;
        check(grid!=nullptr,"Performance worksheet missing");
        if(grid) {
            CalcInput input; TransformerParams params; QString error;
            const bool collected=sample.inputTable()->collectForCalculation(params,input,error,false);
            check(collected,"Valid custom performance criteria failed to collect");
            CalcResult result;
            const bool calculated=collected&&engine.calcElectromagnetic(input,result,true)&&result.valid;
            check(calculated,"Valid custom performance case failed to calculate");
            if(calculated) {
                sample.refreshResult(result);
                const double actual[]={result.winding.loadLoss_W,result.core.noLoadLoss_W,
                    result.core.noLoadCurrent_pct,result.impedance.impedance_pct};
                const double standard[]={params.loadLossStd_W,params.noLoadLossStd_W,
                    params.noLoadCurrentStd_pct,params.impedanceVoltageStd_pct};
                const int columns[]={11,12,14,17};
                for(int i=0;i<4;++i) {
                    const double expected=std::round((actual[i]-standard[i])/standard[i]*10000.0)/100.0;
                    bool ok=false;QString text=grid->item(10,columns[i])->text();text.remove('%');
                    const double value=text.trimmed().toDouble(&ok);
                    check(ok&&std::abs(value-expected)<0.011,"Performance deviation differs from workbook");
                }
                check(grid->item(10,14)->text().contains(QLatin1Char('-')),
                      "No-load current below its standard must show a negative deviation");
                TransformerParams tight=params;
                tight.loadLossStd_W=result.winding.loadLoss_W; tight.loadLossMaxDev_pct=0.0;
                tight.oilTopTempRise_K=result.thermal.oilTopRise_K-1.0;
                auto limits=checkSchemeConstraints(tight,result);
                check(!limits.failedChecks.contains(QStringLiteral("负载损耗"))
                      &&limits.failedChecks.contains(QStringLiteral("油顶层温升K")),
                      "Equality must pass the loss limit and a low oil-top limit must fail");
                tight.loadLossStd_W=result.winding.loadLoss_W-1.0;
                tight.oilTopTempRise_K=result.thermal.oilTopRise_K;
                limits=checkSchemeConstraints(tight,result);
                check(limits.failedChecks.contains(QStringLiteral("负载损耗"))
                      &&!limits.failedChecks.contains(QStringLiteral("油顶层温升K")),
                      "Lowering a loss limit must fail without changing the calculated temperatures");
                CalcInput limitOnly=input;
                limitOnly.performanceCriteria=tight;
                CalcResult unchanged;
                check(engine.calcElectromagnetic(limitOnly,unchanged,true)
                      && unchanged.core.noLoadLoss_W==result.core.noLoadLoss_W
                      && unchanged.winding.loadLoss_W==result.winding.loadLoss_W
                      && unchanged.thermal.oilTopRise_K==result.thermal.oilTopRise_K,
                      "Changing only limits must not change physical results");
                tight.impedanceVoltageStd_pct=result.impedance.impedance_pct;
                tight.impedanceVoltageMinDev_pct=-10; tight.impedanceVoltageMaxDev_pct=10;
                CalcResult outside=result;outside.impedance.impedance_pct=result.impedance.impedance_pct*1.11;
                check(checkSchemeConstraints(tight,outside).failedChecks.contains(QStringLiteral("阻抗电压%")),
                      "Impedance above its upper tolerance must fail");
            }
            grid->item(8,11)->setText(QStringLiteral("0"));
            CalcInput zeroInput;TransformerParams zeroParams;QString zeroError;
            const bool zeroCollected=sample.inputTable()->collectForCalculation(zeroParams,zeroInput,zeroError,false);
            CalcResult zeroResult;
            check(zeroCollected&&engine.calcElectromagnetic(zeroInput,zeroResult,true),"Zero standard case failed");
            check(checkSchemeConstraints(zeroParams,zeroResult).skippedChecks.contains(QStringLiteral("负载损耗"))
                  &&zeroResult.calculationSheetNotes.value(QStringLiteral("L11")).contains(QStringLiteral("标准值为0")),
                  "Zero standard must skip the check and not produce a numeric deviation");
            grid->item(8,11)->setText(QStringLiteral("-1"));
            CalcInput negativeInput;TransformerParams negativeParams;QString negativeError;
            const bool negativeCollected=sample.inputTable()->collectForCalculation(negativeParams,negativeInput,negativeError,false);
            CalcResult negativeResult;
            check(negativeCollected&&engine.calcElectromagnetic(negativeInput,negativeResult,true)
                  &&checkSchemeConstraints(negativeParams,negativeResult).skippedChecks.contains(QStringLiteral("负载损耗"))
                  &&negativeResult.calculationSheetNotes.value(QStringLiteral("L11")).contains(QStringLiteral("标准值无效")),
                  "Negative standard must be identified as invalid and its check skipped");
            grid->item(8,11)->setText(QString());
            CalcInput invalidInput;TransformerParams invalidParams;QString invalidError;
            check(!sample.inputTable()->collectForCalculation(invalidParams,invalidInput,invalidError,false)
                  &&!invalidError.isEmpty(),"Empty performance standard must not silently reuse the old value");
        }
    }
    // Exercise real popup edits against independent workbook expressions.
    {
        const auto roundTo = [](double v, double scale) { return std::round(v * scale) / scale; };
        struct Edit { int row; int col; const char *value; };
        const auto runCase = [&](const QVector<Edit> &edits, int mode) {
            SchemeCalculationSheet sample(TransformerParams{}, StructureConfig{}, CalcInput{}, detailed, true);
            QTableWidget *grid = nullptr;
            for (auto *candidate : sample.findChildren<QTableWidget *>())
                if (candidate->columnCount() == 38) grid = candidate;
            check(grid != nullptr, "Missing grid in winding linkage test");
            if (!grid) return;
            for (const auto &edit : edits) {
                auto *cell = grid->item(edit.row - 1, edit.col - 1);
                check(cell && cell->flags().testFlag(Qt::ItemIsEditable), "Winding input not editable");
                if (!cell) return;
                cell->setText(QString::fromLatin1(edit.value));
            }
            CalcInput input;
            sample.inputTable()->saveToInput(input);
            CalcResult result;
            const bool success = engine.calcElectromagnetic(input, result, true) && result.valid;
            check(success, "Edited winding case did not calculate");
            if (!success) return;
            sample.refreshResult(result);
            const auto number = [&](int row, int col) {
                const auto *cell = grid->item(row-1,col-1);
                bool ok = false;
                const double value = cell ? cell->text().toDouble(&ok) : 0.0;
                check(ok, "Winding result cell is not numeric");
                return value;
            };
            if (mode == 0) {
                const double width=input.hvBareWidth_mm;
                const double radius=width<1.7?0.5:width<2.5?0.65:width<4.0?0.8:1.0;
                const double single=roundTo(width*input.hvBareThick_mm-0.8584*radius*radius,1000.0);
                const double total=single*input.hvParallelCount*input.hvStackCount;
                const double density=roundTo(result.electrical.hvPhaseCurrent_A/total,1000.0);
                const double resistance=roundTo((input.hvCopperWire?0.02135:0.0357)*result.winding.hvWireLenRated_m/total,1000000.0);
                check(std::abs(number(15,21)-single)<0.001, "U15 single-wire cross-section differs from workbook");
                check(std::abs(number(15,27)-total)<0.001, "AA15 total cross-section differs from workbook");
                check(std::abs(number(16,24)-density)<0.001, "X16 current density differs from workbook");
                check(std::abs(number(19,24)-resistance)<0.0000011, "X19 resistance differs from workbook");
            } else if (mode == 1) {
                const double section=input.lvFoilThick_mm*input.lvFoilWidth_mm;
                const double radial=roundTo((input.lvFoilThick_mm*input.lvTurns+(input.lvTurns-1)*input.lvLayerInsulCount*input.lvLayerInsul_mm)*1.05/0.5,1.0)*0.5;
                const double density=roundTo(result.electrical.lvPhaseCurrent_A/section,1000.0);
                const double resistance=roundTo((input.lvCopperFoil?0.02207:0.0357)*result.winding.lvWireLen_m/section,1000000.0);
                check(std::abs(number(15,34)-section)<0.001, "AH15 foil section differs from workbook");
                check(std::abs(number(23,37)-radial)<0.001, "AK23 foil radial thickness differs from workbook");
                check(std::abs(number(16,34)-density)<0.001, "AH16 current density differs from workbook");
                check(std::abs(number(19,34)-resistance)<0.0000011, "AH19 resistance differs from workbook");
            } else {
                const auto detail = [&](const char *address) { return result.calculationSheetDetails.value(QString::fromLatin1(address)).first; };
                double duct=0.0;
                for (int i=0; i<5; ++i) {
                    if (input.lvDuctWidthSide[i] > 0.0) duct+=detail("AK34")*1.7;
                    if (input.lvDuctHeightSide[i] > 0.0) duct+=detail("AK33")*1.7;
                }
                const double inside=result.winding.mainDuct_mm<4.0?0.0:(detail("AK36")+detail("AK39"))*0.85;
                const double area=roundTo(6.0*input.lvFoilWidth_mm*(duct+inside)/1000000.0,100.0);
                double sumLvW=0.0, sumLvH=0.0, sumHvW=0.0, sumHvH=0.0, highDuct=0.0;
                for (int i=0; i<5; ++i) {
                    sumLvW+=input.lvDuctWidthSide[i]; sumLvH+=input.lvDuctHeightSide[i];
                    sumHvW+=input.hvDuctWidthSide[i]; sumHvH+=input.hvDuctHeightSide[i];
                    if (input.hvDuctWidthSide[i]>0.0) highDuct+=detail("AB39");
                    if (input.hvDuctHeightSide[i]>0.0) highDuct+=detail("AB36");
                }
                const double pi=std::acos(-1.0), b29=detail("B29"), b28=input.phaseGapBase_mm;
                const double lvRadial=result.winding.lvRadial_mm, hvRadial=result.winding.hvRadial_mm;
                const double mainDuct=result.winding.mainDuct_mm, majorRadius=result.core.majorRadius_mm;
                const double inner=roundTo(pi*(b29+lvRadial+input.lvDuctHeightSide[0]+input.lvDuctHeightSide[1]
                    +input.lvDuctHeightSide[2]+mainDuct)+detail("AB39"),10.0);
                const double outer=roundTo(pi*(b29+lvRadial+sumLvH+mainDuct+hvRadial+sumHvH)
                    *(180.0-2.0*input.ellipseAngle_deg)/180.0
                    +pi*(majorRadius+b28+lvRadial+sumLvW+mainDuct+hvRadial+sumHvW)
                    *(2.0*input.ellipseAngle_deg)/180.0,10.0);
                const double hvArea=roundTo(6.0*result.winding.hvInnerAxial_mm
                    *(inner*(input.hvRated_kV<=12.0?0.0:0.85)+highDuct*1.7+outer)/1000000.0,100.0);
                check(std::abs(result.thermal.hvSurface_m2-hvArea)<0.011, "AC47 oil-duct surface differs from workbook");
                check(std::abs(number(47,29)-hvArea)<0.011, "AC47 oil-duct surface not refreshed in grid");
                check(result.winding.oilDuctLayoutNote.contains(QStringLiteral("仅单侧启用")),
                      "Single-side oil-duct input must be recorded as a review note");
                check(std::abs(result.thermal.lvSurface_m2-area)<0.011, "AK47 oil-duct surface differs from workbook");
                check(std::abs(number(47,37)-area)<0.011, "AK47 oil-duct surface not refreshed in grid");
                if (input.hvDuctHeightSide[0] == 0.0)
                    check(result.winding.ductLayerIdx[0] == input.hvTurnsPerLayer-1
                          && std::abs(number(30,26)-(input.hvTurnsPerLayer-1))<0.001,
                          "Z30 first duct layer did not follow zero-side workbook branch");
                check(input.oilDuctInputError().isEmpty(), "A zero oil-duct side should remain a valid input");
            }
        };
        // AB13/AB14 = 1x1, 2x1, 1x2, 2x2; then separately change each wire dimension.
        for (const auto pair : {qMakePair("1","1"),qMakePair("2","1"),qMakePair("1","2"),qMakePair("2","2")})
            runCase({{13,28,pair.first},{14,28,pair.second}},0);
        runCase({{13,24,"2.20"}},0);
        runCase({{13,26,"5.70"}},0);
        runCase({{14,32,"1.45"}},1);
        runCase({{14,36,"330"}},1);
        runCase({{26,33,"2"},{26,35,"0.21"}},1);
        runCase({{30,24,"0"}},2); // High-voltage X30 only
        runCase({{30,25,"0"}},2); // High-voltage Y30 only
        runCase({{28,33,"0"}},2); // Low-voltage AG28 only
        runCase({{28,34,"0"}},2); // Low-voltage AH28 only
    }
    // Compare the popup's tank edits and current results with workbook formulas.
    {
        struct TankCase { const char *address; const char *value; };
        const TankCase cases[]={{"F24","100"},{"H25","80"},{"J23","23"},{"J26","330"}};
        const auto roundTo=[](double value,double scale) {return std::round(value*scale)/scale;};
        for (const auto &testCase : cases) {
            SchemeCalculationSheet sample(TransformerParams{}, StructureConfig{}, CalcInput{}, detailed, true);
            QTableWidget *grid=nullptr;
            for (auto *candidate : sample.findChildren<QTableWidget *>()) if(candidate->columnCount()==38)grid=candidate;
            check(grid != nullptr,"Missing tank worksheet grid");
            if(!grid)continue;
            const QString addr=QString::fromLatin1(testCase.address);
            const QString val=QString::fromLatin1(testCase.value);
            auto *edit=sample.findChild<QLineEdit *>(QStringLiteral("tankInput_")+addr);
            check(edit != nullptr,"Tank dimension editor missing");
            if(!edit)continue;
            edit->setText(val);
            QMetaObject::invokeMethod(edit,"textEdited",Qt::DirectConnection,Q_ARG(QString,val));
            CalcInput input; sample.inputTable()->saveToInput(input);
            const double expected=val.toDouble();
            const double collected=addr==QStringLiteral("F24")?input.tankSideClear_mm
                :addr==QStringLiteral("H25")?input.tankEndClear_mm
                :addr==QStringLiteral("J23")?input.tankBottomOil_mm
                :input.tankFoot_mm;
            check(std::abs(collected-expected)<0.00001,"Edited tank input did not reach calculation");
            CalcResult result;
            const bool ok=engine.calcElectromagnetic(input,result,true)&&result.valid;
            check(ok,"Edited tank case did not calculate");
            if(!ok)continue;
            sample.refreshResult(result);
            const auto detail=[&](const char *name) {
                const auto key=QString::fromLatin1(name);
                check(result.calculationSheetDetails.contains(key),"Missing workbook intermediate value");
                return result.calculationSheetDetails.value(key).first;
            };
            const auto shown=[&](int row,int col) {
                const auto *cell=grid->item(row-1,col-1);
                bool valid=false;const double value=cell?cell->text().toDouble(&valid):0.0;
                check(valid,"Tank or thermal display not numeric");return value;
            };
            const auto label=[&](const char *name) {
                const auto *text=sample.findChild<QLabel *>(QString::fromLatin1(name));
                bool valid=false;const double value=text?text->text().toDouble(&valid):0.0;
                check(valid,"Tank dimension label not numeric");return value;
            };
            const double width=detail("D53")+input.tankSideClear_mm;
            const double length=2.0*result.mass.centerDistance_mm+detail("B47")+input.tankEndClear_mm;
            const double height=input.tankBottomOil_mm+2.0*result.core.widths_mm.first()
                +result.winding.lvAxial_mm+input.tankFoot_mm;
            check(std::abs(label("tankWidthValue")-width)<0.01,"F25 width differs from workbook");
            check(std::abs(label("tankLengthValue")-length)<0.01,"H26 length differs from workbook");
            check(std::abs(label("tankHeightValue")-height)<0.01,"J27 height differs from workbook");
            const double c20=roundTo(length*width*height*0.9/1000000.0,1.0);
            const double c21=roundTo(result.core.coreWeight_kg/7.8
                +result.winding.hvWireWeight_kg/(input.hvCopperWire?4.5:1.85)
                +result.winding.lvWireWeight_kg/(input.lvCopperFoil?4.5:1.85),1.0);
            const double c22=roundTo(6.0*input.waveHeight_mm*input.waveDepth_mm*0.9
                *(result.oilExpansion.longSideCount+result.oilExpansion.shortSideCount)*2.0/1000000.0,1.0);
            const double c19=roundTo((detail("D12")+detail("D13")+detail("D14")+detail("D15"))*detail("D16")+detail("D17"),1.0);
            check(std::abs(shown(19,3)-c19)<0.01,"C19 tank weight differs from workbook zero-conservator case");
            check(std::abs(shown(20,3)-c20)<0.01,"C20 empty-tank oil differs from workbook");
            check(std::abs(shown(21,3)-c21)<0.01,"C21 displaced oil differs from workbook");
            check(std::abs(shown(22,3)-c22)<0.01,"C22 cooling oil differs from workbook");
            check(std::abs(shown(24,3)-(c20-c21+c22))<0.01,"C24 total oil differs from workbook zero-conservator case");
            check(std::abs(shown(25,3)-(c19+(c20-c21+c22)+result.mass.activePartWeight_kg))<0.01,
                "C25 total weight differs from workbook");
            const double oilRise=roundTo(0.262*std::pow(detail("I49"),0.8),10.0);
            const double oilTop=roundTo(1.2*oilRise+input.oilTopCorrection_K,10.0);
            check(std::abs(shown(49,14)-oilRise)<0.01,"N49 oil rise differs from workbook");
            check(std::abs(shown(51,14)-oilTop)<0.01,"N51 top oil rise differs from workbook");
            const double highAboveOil=result.thermal.hvSurfaceRise_K+result.thermal.hvGapCorrection_K+std::max(0.0,result.thermal.hvLayerCorrection_K);
            const double lowAboveOil=result.thermal.lvSurfaceRise_K+std::max(0.0,result.thermal.lvLayerCorrection_K);
            check(std::abs(shown(54,25)-(highAboveOil+oilRise))<0.01,"Y54 high winding rise differs from workbook");
            check(std::abs(shown(52,37)-(lowAboveOil+oilRise))<0.01,"AK52 low winding rise differs from workbook");
        }
    }
    // The complete original sheet must fit the available width without horizontal scrolling.
    {
        SchemeCalculationSheet responsive(TransformerParams{},StructureConfig{},CalcInput{},detailed,true);
        QTableWidget *grid=nullptr;
        for(auto *candidate:responsive.findChildren<QTableWidget *>())if(candidate->columnCount()==38)grid=candidate;
        check(grid!=nullptr,"Responsive sheet grid missing");
        if(grid) {
            responsive.show();
            int initialHeight=-1;
            for(int width : {1700,1280,1920,1700}) {
                responsive.resize(width,900);
                for(int i=0;i<4;++i)app.processEvents();
                if(width==1700 && qEnvironmentVariableIsSet("ZTBLD_COMPACT_IMAGE")) {
                    responsive.grab().save(qEnvironmentVariable("ZTBLD_COMPACT_IMAGE"));
                    grid->scrollToItem(grid->item(20,4),QAbstractItemView::PositionAtTop);
                    app.processEvents();
                    responsive.grab().save(qEnvironmentVariable("ZTBLD_COMPACT_IMAGE")+QStringLiteral(".tank.png"));
                    grid->scrollToTop();app.processEvents();
                }
                if(width>=1700) {
                    check(grid->columnWidth(0)>=grid->fontMetrics().horizontalAdvance(QStringLiteral("主空道"))+20,
                          "Main duct and phase gap labels should fit at desktop width");
                    check(grid->columnWidth(4)>=grid->fontMetrics().horizontalAdvance(QStringLiteral("标准"))+8,
                          "Standard label should fit at desktop width");
                    check(grid->columnWidth(28)>=grid->fontMetrics().horizontalAdvance(QStringLiteral("扁线"))+16,
                          "Wire-type dropdown should fit at desktop width");
                    check(grid->columnWidth(16)>=grid->fontMetrics().horizontalAdvance(QStringLiteral("大圆心角"))+6,
                          "Core angle label should fit at desktop width");
                }
                if(width==1700) {
                    if(initialHeight<0)initialHeight=grid->rowHeight(4);
                    else check(grid->rowHeight(4)<=initialHeight+1,
                               "Widening the window must restore compact row heights");
                }
                check(grid->horizontalScrollBar()->maximum()==0,"Sheet should fit without horizontal scrolling");
                check(grid->visualItemRect(grid->item(11,37)).right()<=grid->viewport()->width(),
                      "The rightmost cell should remain in the viewport");
                check(grid->item(29,23)->flags().testFlag(Qt::ItemIsEditable),
                      "Resizing must preserve the original editable duct input");
            }
            responsive.hide();
        }
    }
    {
        CalcResult snapshot=baseline;
        snapshot.winding.hvCoilFormIdx=2; snapshot.winding.hvSegmentGap_mm=35.0;
        const auto report=ElectromagneticEngine::buildPrintOutput(CalcInput{}, snapshot);
        bool form=false, gap=false;
        for(const auto &row:report.rows) {
            if(row.leftName==QStringLiteral("高压线圈型式") && row.leftValue==QStringLiteral("分段式")) form=true;
            if(row.rightName==QStringLiteral("高压段间距") && row.rightValue==QStringLiteral("35.00")) gap=true;
        }
        check(form && gap, "Print report must show coil type and segment gap from result snapshot");
    }
    {
        CalcInput segmented; segmented.hvCoilFormIdx = 2;
        const auto preview = ElectromagneticEngine::previewSegmentWinding(segmented);
        check(preview.windingAvailable && preview.values.contains(QStringLiteral("W28"))
              && std::abs(preview.values.value(QStringLiteral("W28")).first - 38.0) < 0.001
              && std::abs(preview.values.value(QStringLiteral("AA29")).first - 35.0) < 0.001,
              "Segmented preview must match workbook winding dimensions");
        check(preview.values.contains(QStringLiteral("R10"))
              && std::abs(preview.values.value(QStringLiteral("R10")).first - 7.56) < 0.001,
              "Segmented preview impedance must match workbook");
        check(preview.values.contains(QStringLiteral("M10")) && preview.values.value(QStringLiteral("M10")).first == 524
              && preview.values.contains(QStringLiteral("N51")) && std::abs(preview.values.value(QStringLiteral("N51")).first-37.9)<0.001,
              "Segmented preview must use current steel curve and show oil temperature");
        check(preview.values.value(QStringLiteral("C9")).first==905 && preview.values.value(QStringLiteral("C10")).first==503
              && preview.values.value(QStringLiteral("C25")).first==2561
              && std::abs(preview.values.value(QStringLiteral("H26")).first-1321)<0.001
              && std::abs(preview.values.value(QStringLiteral("F25")).first-554)<0.001
              && std::abs(preview.values.value(QStringLiteral("materialCostPreview")).first-62788.0)<0.001,
              "Segmented preview must calculate weights, tank dimensions and material cost");
        CalcInput stoppedDuct=segmented; stoppedDuct.hvDuctHeightSide[1]=0;
        const auto stoppedPreview = ElectromagneticEngine::previewSegmentWinding(stoppedDuct);
        check(!stoppedPreview.values.contains(QStringLiteral("R10")) && !stoppedPreview.values.contains(QStringLiteral("N51")),
              "Unverified second duct must not expose impedance or temperature");
        CalcInput badPrices=segmented; badPrices.materialPrices.custom=true; badPrices.materialPrices.copper=-1;
        const auto badPricesPreview=ElectromagneticEngine::previewSegmentWinding(badPrices);
        check(badPricesPreview.values.contains(QStringLiteral("C25")) && !badPricesPreview.values.contains(QStringLiteral("materialCostPreview")),
              "Invalid prices must hide material cost without hiding valid weights");
        CalcInput badThermal=segmented; badThermal.wavePitch_mm=0;
        const auto badThermalPreview=ElectromagneticEngine::previewSegmentWinding(badThermal);
        check(badThermalPreview.values.contains(QStringLiteral("R10")) && !badThermalPreview.values.contains(QStringLiteral("N51")),
              "Invalid oil-tank pitch must hide temperature and preserve independent impedance");
        CalcResult segmentResult;
        check(engine.calcElectromagnetic(segmented, segmentResult, true) && segmentResult.valid
              && std::abs(segmentResult.impedance.impedance_pct-7.56)<0.001,
              "Verified normal-duct segmented full calculation must match workbook");
        CalcInput invalid=segmented; invalid.hvDuctHeightSide[1]=0;
        CalcResult invalidResult;
        check(!engine.calcElectromagnetic(invalid,invalidResult), "Unverified segmented duct layout must remain blocked");
        StructureConfig segmentConfig;segmentConfig.hvCoilStructure=StructureConfig::TwoSegCylinder;
        check(calculationScopeError(segmentConfig,TransformerParams{},segmented).isEmpty(),
              "Matching segmented selection must pass shared scope check");
        check(!calculationScopeError(StructureConfig{},TransformerParams{},segmented).isEmpty(),
              "Mismatched selector and segmented input must be rejected");
    }
    {
        SchemeCalculationSheet sheet(TransformerParams{},StructureConfig{},CalcInput{},detailed,true);
        auto *selector=sheet.findChild<QComboBox *>(QStringLiteral("sheetCoilForm"));
        check(selector&&selector->count()==2&&selector->currentData().toInt()==1,
              "High-voltage coil selector must show the original two options");
        if(selector) {
            selector->setCurrentIndex(1);
            QString error;
            CalcInput selectedForm;
            check(sheet.captureCoilForm(selectedForm, error) && selectedForm.hvCoilFormIdx == 2,
                  "Selecting segmented form must propagate the form without enabling calculation");
            check(sheet.inputsReady(error) && sheet.inputTable()->calculationConfig().hvCoilStructure==StructureConfig::TwoSegCylinder,
                  "Verified segmented form must be selectable and synchronize config");
            sheet.loadResult(detailed);
            QTableWidget *currentGrid=nullptr;
            for(auto *candidate:sheet.findChildren<QTableWidget *>())if(candidate->columnCount()==38)currentGrid=candidate;
            selector=currentGrid?qobject_cast<QComboBox *>(currentGrid->cellWidget(8,28)):nullptr;
            check(selector&&selector->currentData().toInt()==2&&sheet.inputsReady(error)
                  && currentGrid->item(9,17)->text().contains(QStringLiteral("待重新计算")),
                  "Refreshing must not reset coil selection or show a stale result");
            if(selector) {
                selector->setCurrentIndex(0);
                check(sheet.inputsReady(error),"Switching back to cylinder must recover the supported input");
                sheet.refreshResult(detailed);
                check(sheet.partialPreviewStatus().isEmpty(),"Supported coil must restore recalculated results");
            }
        }
    }
    {
        TransformerParams matching; matching.productModel = QStringLiteral("SB20-M-630/10-0.4");
        matching.standardMode = TransformerParams::StandardMode::Custom;
        SchemeCalculationSheet segmentSheet(matching,StructureConfig{},CalcInput{},detailed,true);
        auto *selector=segmentSheet.findChild<QComboBox *>(QStringLiteral("sheetCoilForm"));
        QTableWidget *grid=nullptr;
        for(auto *candidate:segmentSheet.findChildren<QTableWidget *>()) if(candidate->columnCount()==38) grid=candidate;
        check(selector && grid, "Segmented calculation controls must exist");
        if(selector && grid) {
            selector->setCurrentIndex(1);
            check(grid->item(9,17) && grid->item(9,17)->text().contains(QStringLiteral("待重新计算")),
                  "Switching type must hide old impedance until recalculation");
            const auto refresh=[&](){
                CalcInput current; TransformerParams params; QString error; CalcResult result;
                const bool ok=segmentSheet.captureCoilForm(current,error) && segmentSheet.inputsReady(error)
                    && segmentSheet.inputTable()->collectForCalculation(params,current,error,false)
                    && engine.calcElectromagnetic(current,result,true);
                if(ok)segmentSheet.refreshResult(result);
                return ok;
            };
            check(refresh(), "Segmented sheet must collect and calculate valid parameters");
            check(grid->item(9,17) && std::abs(grid->item(9,17)->text().toDouble()-7.56)<0.001,
                  "Segmented sheet must show recalculated impedance");
            if(grid->item(11,22))grid->item(11,22)->setText(QStringLiteral("12"));
            check(refresh(), "Edited segmented layer count must recalculate");
            check(grid->item(9,17) && std::abs(grid->item(9,17)->text().toDouble()-7.91)<0.001
                  && grid->item(24,2) && grid->item(24,2)->text().toDouble()==2468,
                  "Edited segmented values must update together");
            if(grid->item(30,24))grid->item(30,24)->setText(QStringLiteral("0"));
            QString error;
            check(!segmentSheet.inputsReady(error) && !error.isEmpty(),
                  "Unverified segmented oil-duct layout must block confirmation");
            check(grid->item(9,17) && grid->item(9,17)->text().contains(QStringLiteral("待核对")),
                  "Unverified oil-duct layout must hide old impedance");
        }
    }
    SchemeCalculationSheet linked(TransformerParams{}, StructureConfig{}, CalcInput{}, detailed, true);
    QTableWidget *linkedGrid = nullptr;
    for (auto *candidate : linked.findChildren<QTableWidget *>())
        if (candidate->columnCount() == 38) linkedGrid = candidate;
    check(linkedGrid != nullptr, "Missing original worksheet grid");
    if (linkedGrid) {
        check(linkedGrid->rowCount() == 54, "Original sheet must have exactly 54 rows");
        auto *supplement = linked.findChild<QPushButton *>(QStringLiteral("supplementParametersButton"));
        auto *source = linked.inputTable();
        int seamRow = -1, lossLimitRow = -1;
        for (int row = 0; row < source->rowCount(); ++row) {
            const auto *label = source->item(row, 1);
            if (label && label->text() == QStringLiteral("接缝数")) seamRow = row;
            if (label && label->text() == QStringLiteral("油顶层温升限值(K)")) lossLimitRow = row;
        }
        check(supplement && seamRow >= 0 && lossLimitRow >= 0, "Supplementary input fields missing");
        if (supplement && seamRow >= 0 && lossLimitRow >= 0) {
            const QString previous = source->item(seamRow, 2)->text();
            const QString previousLimit = source->item(lossLimitRow, 2)->text();
            QTimer::singleShot(0, &linked, [&]() {
                auto *dialog = linked.findChild<QDialog *>(QStringLiteral("supplementParametersDialog"));
                check(dialog != nullptr, "Supplement dialog failed to open");
                if (!dialog) return;
                check(!source->isRowHidden(seamRow) && !source->isRowHidden(lossLimitRow),
                      "Calculation or constraint input not accessible");
                check(dialog->findChild<QTextBrowser *>(QStringLiteral("supplementDetails")) != nullptr,
                      "Supplement calculation notes missing");
                if (qEnvironmentVariableIsSet("ZTBLD_SUPPLEMENT_IMAGE")) {
                    QApplication::processEvents();
                    dialog->grab().save(qEnvironmentVariable("ZTBLD_SUPPLEMENT_IMAGE"));
                }
                source->item(seamRow, 2)->setText(QStringLiteral("7"));
                source->item(lossLimitRow, 2)->setText(QStringLiteral("62"));
                dialog->accept();
            });
            supplement->click();
            CalcInput changed;
            source->saveToInput(changed);
            check(changed.seamCount == 7, "Supplement input did not reach calculation source");
            check(std::abs(source->getParams().oilTopTempRise_K - 62.0) < 1e-9, "Supplement limit did not reach validation settings");
            check(source->parent() == &linked && source->isHidden(), "Source table not restored after closing dialog");
            source->item(seamRow, 2)->setText(previous);
            source->item(lossLimitRow, 2)->setText(previousLimit);
        }
        // Verify original-sheet geometry with independent workbook formulas.
        {
            const struct { int col; double value; } edits[] = {{7, 180.0}, {10, 110.0}, {12, 20.0}};
            for (const auto &edit : edits) {
                linkedGrid->item(11, edit.col)->setText(QString::number(edit.value));
                CalcInput input;
                linked.inputTable()->saveToInput(input);
                CalcResult result;
                const bool success = engine.calcElectromagnetic(input, result, true) && result.valid;
                if(!success)qWarning().noquote()<<result.error;
                check(success, "Core geometry edit failed to recalculate");
                if (!success) continue;
                linked.refreshResult(result);
                const double pi = std::acos(-1.0);
                const double radians = input.ellipseAngle_deg * pi / 180.0;
                const double radius = std::round((input.coreDiameter_mm / 2.0
                    + input.coreStraight_mm / (2.0 * std::sin(radians))) * 100.0) / 100.0;
                bool okRadius = false, okSmall = false, okLarge = false;
                const double shownRadius = linkedGrid->item(19,12)->text().toDouble(&okRadius);
                const double shownSmall = linkedGrid->item(11,15)->text().toDouble(&okSmall);
                const double shownLarge = linkedGrid->item(11,18)->text().toDouble(&okLarge);
                check(okRadius && std::abs(shownRadius-radius)<0.011,
                      "M20 radius must match the original workbook formula after an edit");
                check(okSmall && okLarge && std::abs(shownSmall-(180.0-2.0*input.ellipseAngle_deg))<0.01
                      && std::abs(shownLarge-2.0*input.ellipseAngle_deg)<0.01,
                      "P12/S12 angles must match the original workbook formulas");
            }
            linkedGrid->item(11,7)->setText(QStringLiteral("170"));
            linkedGrid->item(11,10)->setText(QStringLiteral("103"));
            linkedGrid->item(11,12)->setText(QStringLiteral("18"));
            linked.refreshResult(detailed);
        }
        {
            CalcResult older = detailed;
            older.calculationSheetNotes[QStringLiteral("AJ45")] = QStringLiteral("见下方高级工艺参数“低压附加损耗(W)”");
            SchemeCalculationSheet oldSheet(TransformerParams{}, StructureConfig{}, CalcInput{}, older, true);
            QTableWidget *oldGrid = nullptr;
            for (auto *candidate : oldSheet.findChildren<QTableWidget *>())
                if (candidate->columnCount() == 38) oldGrid = candidate;
            auto *cell = oldGrid ? oldGrid->item(44,35) : nullptr;
            check(cell && cell->text() == QString::number(detailed.winding.lvExtraLoss_W,'g',12)
                      && cell->toolTip().contains(QStringLiteral("顶部“补充参数”"))
                      && !cell->toolTip().contains(QStringLiteral("下方高级工艺参数")),
                  "Older saved AJ45 notes must point to supplementary parameters");
            auto *freshCell = linkedGrid->item(44,35);
            check(freshCell && freshCell->toolTip().contains(QStringLiteral("顶部“补充参数”")),
                  "Fresh AJ45 notes must point to supplementary parameters");
        }
        for (int row=29; row<=33; ++row) for (int col : {23,24}) {
            const auto *item=linkedGrid->item(row,col);
            check(item && item->flags().testFlag(Qt::ItemIsEditable),
                  "High voltage duct input must be editable at its original address");
            check(item && item->background().color() == QColor("#FF00FF"),
                  "High voltage duct input must use the magenta background");
        }
        for (int row=27; row<=31; ++row) for (int col : {32,33}) {
            const auto *item=linkedGrid->item(row,col);
            check(item && item->flags().testFlag(Qt::ItemIsEditable)
                      && item->background().color() == QColor("#FF00FF"),
                  "Low voltage duct input must be editable and magenta");
        }
        linked.resize(1800,1000); linked.show();
        for(int i=0;i<4;++i)app.processEvents();
        linkedGrid->scrollToItem(linkedGrid->item(29,23));
        app.processEvents();
        const QRect ductRect=linkedGrid->visualItemRect(linkedGrid->item(29,23));
        const QImage rendered=linkedGrid->viewport()->grab().toImage();
        if(qEnvironmentVariableIsSet("ZTBLD_DUCT_IMAGE")) {
            rendered.save(qEnvironmentVariable("ZTBLD_DUCT_IMAGE"));
            qInfo()<<"duct rect"<<ductRect<<"viewport"<<rendered.size()
                   <<"color"<<rendered.pixelColor(ductRect.topLeft()+QPoint(5,5));
        }
        check(ductRect.isValid() && rendered.rect().contains(ductRect.topLeft()+QPoint(5,5))
                  && rendered.pixelColor(ductRect.topLeft()+QPoint(5,5)) == QColor("#FF00FF"),
              "Production theme must render the high-voltage duct cell magenta");
        linked.hide();
        const auto *oilCorrection=linkedGrid->item(50,11);
        check(oilCorrection && !oilCorrection->flags().testFlag(Qt::ItemIsEditable),
              "Fixed oil top correction L51 must be read-only");
        check(linkedGrid->item(7,35) && linkedGrid->item(7,35)->flags().testFlag(Qt::ItemIsEditable),
              "Reference flux must be editable beside the LV turns");
        linkedGrid->item(29,23)->setText(QStringLiteral("5"));
        linkedGrid->item(7,35)->setText(QStringLiteral("1.65"));
        CalcInput sheetEdit;
        linked.inputTable()->saveToInput(sheetEdit);
        const auto savedInput = SchemeStore::toJson(sheetEdit);
        check(!savedInput.contains("oilTopCorrection_K")
                  &&sheetEdit.hvDuctWidthSide[0]==5.0&&sheetEdit.refFluxDens_T==1.65,
              "Real original-sheet inputs must update the scheme without saving fixed defaults");
        const auto restoredEdit=SchemeStore::fromJson(savedInput);
        check(restoredEdit.oilTopCorrection_K==6.0&&restoredEdit.hvDuctWidthSide[0]==5.0
                  &&restoredEdit.refFluxDens_T==1.65,
              "Oil duct and reference flux must survive save/reload with fixed oil correction");
        linkedGrid->item(29,23)->setText(QStringLiteral("4"));
        linkedGrid->item(7,35)->setText(QStringLiteral("1.7"));
        for(int row:{36,47}) {
            check(linkedGrid->columnSpan(row,0)==1 && linkedGrid->columnSpan(row,1)==1,
                  "Main duct and phase gap must preserve one original name cell and one original value cell");
            const QString source=row==36?QStringLiteral("B37"):QStringLiteral("B48");
            bool numeric=false;
            auto *value=linkedGrid->item(row,1);
            const double actual=value?value->text().toDouble(&numeric):0;
            const double expected=row==36?detailed.winding.mainDuct_mm:detailed.calculationSheetDetails.value(source).first;
            check(numeric && std::abs(actual-expected)<0.00001,
                  "Single-cell duct/gap fields must retain their original numeric result bindings");
        }
        for(int row:{22,23,24})check(linkedGrid->columnSpan(row,2)==2,
              "Oil and transformer weight values must match the two-column values above");
        check(linkedGrid->columnSpan(25,0)==4,
              "Centre-distance heading must occupy exactly the two name/value blocks above");
        const auto originalLayout=QJsonDocument::fromJson(calculationSheetTemplate).object();
        QHash<int,QPair<int,int>> originalSpans;QSet<int> covered;
        for(const auto &entry:originalLayout["merges"].toArray()) {
            const auto m=entry.toArray();const int row=m[0].toInt(),col=m[1].toInt(),h=m[2].toInt(),w=m[3].toInt();
            originalSpans.insert(row*38+col,{h,w});
            for(int y=row;y<row+h;++y)for(int x=col;x<col+w;++x)
                if(y!=row||x!=col)covered.insert(y*38+x);
        }
        bool originalGrid=true;
        for(int row=26;row<54;++row)for(int col=0;col<38;++col) {
            if(row==26&&col>=4&&col<=18)continue; // Authorized independent tank panel.
            if(col==0&&((row>=32&&row<=34)||(row>=41&&row<=43)||(row>=45&&row<=46)))continue; // Source border-only blank blocks.
            if((col==1&&row>=41&&row<=43)||((col==2||col==3)&&row>=32&&row<=34)
               ||(col==3&&row>=48&&row<=49))continue; // Source border-only left blocks.
            if(col==19)continue; // T-column groups follow actual source borders, not Excel gridlines.
            if(covered.contains(row*38+col))continue;
            const auto span=originalSpans.value(row*38+col,{1,1});
            originalGrid&=linkedGrid->rowSpan(row,col)==span.first&&linkedGrid->columnSpan(row,col)==span.second;
        }
        check(originalGrid,"Row 27 onward must retain workbook merges and separate blank cells exactly");
        auto *tank=linkedGrid->cellWidget(20,4);
        check(tank&&tank->objectName()==QStringLiteral("tankDimensionsPanel")
                  &&linkedGrid->rowSpan(20,4)==7&&linkedGrid->columnSpan(20,4)==15,
              "Tank area must be a continuous bordered panel instead of a bare spreadsheet grid");
        auto *length=tank?tank->findChild<QLabel *>("tankLengthValue"):nullptr;
        auto *width=tank?tank->findChild<QLabel *>("tankWidthValue"):nullptr;
        auto *height=tank?tank->findChild<QLabel *>("tankHeightValue"):nullptr;
        check(length&&width&&height&&length->text().toDouble()==detailed.mass.tankLength_mm
                  &&width->text().toDouble()==detailed.mass.tankWidth_mm&&height->text().toDouble()==detailed.mass.tankHeight_mm,
              "Tank length/width/height headings must directly show the matching current result");
        check(tank&&tank->findChild<QWidget *>("tankSketch"),"Tank panel must contain a visible line drawing");
        auto *sketch=tank?tank->findChild<QWidget *>("tankSketch"):nullptr;
        check(sketch&&sketch->accessibleName().contains(QStringLiteral("波纹壁截面")),
              "The second tank drawing must be a corrugated-wall section, not an overall tank side view");
        auto *tankForm=linked.findChild<QComboBox *>("sheetTankForm");
        check(tankForm&&tankForm->count()==2&&tankForm->itemText(0)==QStringLiteral("波纹油箱")
                  &&tankForm->itemText(1)==QStringLiteral("片散油箱"),
              "Tank form must expose the two original workbook dropdown choices");
        if(tankForm){
            CalcInput before;linked.inputTable()->saveToInput(before);
            tankForm->setCurrentIndex(1);QString error;
            check(!linked.inputsReady(error)&&error.contains(QStringLiteral("片散油箱")),
                  "Radiator selection cannot confirm using corrugated tank calculation");
            check(height&&height->text().contains(QStringLiteral("待核对")),
                  "Radiator selection must clear old tank dimensions and geometry");
            linked.refreshInputValues();
            check(tankForm->currentIndex()==1&&linkedGrid->item(20,12)->text()==QStringLiteral("片散油箱"),
                  "Input refresh cannot silently reset the selected tank form");
            linked.loadResult(detailed);tankForm=linked.findChild<QComboBox *>("sheetTankForm");
            // Deferred deletion of old index widgets can leave old children until
            // the next event loop; choose the editor owned by the current panel.
            tank=linkedGrid->cellWidget(20,4);tankForm=tank?tank->findChild<QComboBox *>("sheetTankForm"):nullptr;
            check(tankForm&&tankForm->currentIndex()==1,"Same-dialog result reload preserves inspection selection");
            if(tankForm){tankForm->setCurrentIndex(0);linked.refreshResult(detailed);}
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            length=tank?tank->findChild<QLabel *>("tankLengthValue"):nullptr;
            width=tank?tank->findChild<QLabel *>("tankWidthValue"):nullptr;
            height=tank?tank->findChild<QLabel *>("tankHeightValue"):nullptr;
            sketch=tank?tank->findChild<QWidget *>("tankSketch"):nullptr;
            CalcInput after;linked.inputTable()->saveToInput(after);
            check(SchemeStore::toJson(before)==SchemeStore::toJson(after),"Tank inspection choices cannot alter underlying design parameters");
            check(linked.inputsReady(error),"Returning to corrugated tank restores current calculation readiness");
        }
        check(sketch&&sketch->accessibleDescription().contains(QStringLiteral("箱沿高"))
                  &&sketch->accessibleDescription().contains(QStringLiteral("箱顶油空"))
                  &&!sketch->accessibleDescription().contains(QStringLiteral("底留空")),
              "Sketch annotations must preserve Q22 rim-height and Q26 top-oil-clearance meanings");
        check(linkedGrid->columnSpan(16,23)==6 && linkedGrid->columnSpan(16,33)==5,
              "Mean-turn-length fields must each use one continuous cell without blank grid subdivisions");
        check(linkedGrid->columnSpan(18,0)==1 && linkedGrid->columnSpan(18,1)==1,
              "Tank accessory weight label must preserve the source A19 single-column boundary");
        check(linkedGrid->wordWrap() && linkedGrid->textElideMode()==Qt::ElideNone,
              "Worksheet text must wrap rather than disappear behind ellipses");
        check(linkedGrid->columnWidth(8)>=linkedGrid->fontMetrics().horizontalAdvance("000.00")+12,
              "Narrow numeric fields must fit a six-character value plus cell padding");
    }
    if (linkedGrid) {
        const auto numberAt = [&](int row, int col, double expected) {
            bool numeric = false;
            const auto *item = linkedGrid->item(row, col);
            const double value = item ? item->text().toDouble(&numeric) : 0.0;
            return numeric && std::abs(value - expected) < 0.00001;
        };
        const auto unavailableAt = [&](int row, int col) {
            const auto *item = linkedGrid->item(row, col);
            return item && item->text().contains(QStringLiteral("待核对"));
        };
        auto refreshed=detailed;
        refreshed.winding.mainDuct_mm=6.5;
        refreshed.calculationSheetDetails[QStringLiteral("B48")].first=18.0;
        auto *ductCell=linkedGrid->item(36,1);
        auto *gapCell=linkedGrid->item(47,1);
        linked.refreshResult(refreshed);
        check(numberAt(36,1,6.5)&&numberAt(47,1,18.0)
                  &&linkedGrid->item(36,1)==ductCell&&linkedGrid->item(47,1)==gapCell,
              "Refreshed workbook B37/B48 results must update their original cells without rebuilding");
        linked.refreshResult(detailed);
        auto *bottomEdit=linked.findChild<QLineEdit *>("tankInput_J23");
        check(bottomEdit,"Tank bottom clearance input must remain editable in the independent panel");
        if(bottomEdit){
            bottomEdit->setText(QStringLiteral("23"));
            QMetaObject::invokeMethod(bottomEdit,"textEdited",Qt::DirectConnection,Q_ARG(QString,QStringLiteral("23")));
            CalcInput changed;linked.inputTable()->saveToInput(changed);
            check(changed.tankBottomOil_mm==23.0,"Tank panel edits must reach the original authoritative input");
            CalcResult recalculated;check(engine.calcElectromagnetic(changed,recalculated,true),"Panel edit remains calculable");
            linked.refreshResult(recalculated);
            auto *height=linked.findChild<QLabel *>("tankHeightValue");
            check(height&&height->text().toDouble()==recalculated.mass.tankHeight_mm
                      &&linked.findChild<QLineEdit *>("tankInput_J23")==bottomEdit,
                  "Recalculation must update the panel while preserving its live editor");
            bottomEdit->setText(QStringLiteral("19"));
            QMetaObject::invokeMethod(bottomEdit,"textEdited",Qt::DirectConnection,Q_ARG(QString,QStringLiteral("19")));
            linked.refreshResult(detailed);
        }
        const auto tankImage=qEnvironmentVariable("ZTBLD_TANK_PANEL_IMAGE");
        if(!tankImage.isEmpty()){
            linked.resize(2300,1800);linked.show();app.processEvents();
            auto *tank=linkedGrid->cellWidget(20,4);
            check(tank&&tank->grab().save(tankImage),"Save tank panel visual check");linked.hide();
        }
        auto *linkedShape = qobject_cast<QComboBox *>(linkedGrid->cellWidget(3, 5));
        linkedShape->setCurrentIndex(0); // circle, keep K12=103 until explicitly changed
        auto *partialTankHeight=linked.findChild<QLabel *>("tankHeightValue");
        check(partialTankHeight&&partialTankHeight->text().contains(QStringLiteral("待核对")),
              "Partial structure cannot retain a seemingly current complete tank size");
        check(numberAt(16, 6, 165.0) && numberAt(17, 6, 20.0),
              "Circle selection must immediately refresh G17/G18 from original geometry");
        check(unavailableAt(46, 28) && unavailableAt(50, 13) && unavailableAt(13, 13),
              "Partial preview must clear ellipse AC47/N51/N14 instead of keeping stale results");
        check(unavailableAt(16, 23), "Circle with nonzero K12 must not silently compute circular mean turns");
        linkedShape->setCurrentIndex(1); // long round, K12=103 explicitly contributes to mean turns
        check(numberAt(16, 5, 170.0) && numberAt(17, 5, 103.0),
              "Long round must display central rectangle F17/F18");
        auto *combinationTank=linkedGrid->cellWidget(20,4)->findChild<QComboBox *>("sheetTankForm");
        if(combinationTank){
            combinationTank->setCurrentIndex(1);
            check(unavailableAt(17,5),"Radiator inspection must clear the separately updated long-round F18 result");
            linkedGrid->item(11,10)->setText(QStringLiteral("113"));
            check(unavailableAt(17,5),"Editing long-round geometry during radiator inspection must not resurrect stale F18");
            linkedGrid->item(11,10)->setText(QStringLiteral("103"));
            combinationTank->setCurrentIndex(0);
            check(numberAt(17,5,103),"Returning to corrugated tank recovers current long-round partial geometry");
        }
        bool turnNumeric = false;
        const double oldTurn = linkedGrid->item(16, 23)->text().toDouble(&turnNumeric);
        check(turnNumeric && oldTurn > 0.0,
              "Long-round X17 must show a fresh numeric mean turn length");
        linkedGrid->item(11, 10)->setText(QStringLiteral("113")); // K12
        check(numberAt(16, 23, oldTurn + 0.02),
              "Changing long-round K12 by 10mm must increase full mean turn by 0.02m");
        check(numberAt(38, 27, 113.0), "Long-round AB39 must follow K12, not ellipse radius");
        const auto *resistance = linkedGrid->item(18, 23); // X19
        check(resistance && resistance->text().toDouble() > 0.0,
              "Verified downstream winding resistance must refresh in place");
        linkedGrid->item(25, 34)->setText(QStringLiteral("bad")); // AI26, actual binding lvLayerInsul_mm
        check(unavailableAt(16, 23), "Invalid low-voltage layer insulation must not reuse the saved thickness");
        linkedGrid->item(25, 34)->setText(QStringLiteral("0.18"));
        check(numberAt(16, 23, oldTurn + 0.02), "Valid low-voltage insulation must restore current winding preview");
        auto *tapContainer = linkedGrid->cellWidget(2, 9);
        auto *visibleStep = tapContainer ? tapContainer->findChild<QDoubleSpinBox *>() : nullptr;
        check(visibleStep != nullptr, "Missing mirrored tap step editor");
        if (visibleStep) {
            auto *tapEdit = visibleStep->findChild<QLineEdit *>();
            tapEdit->setText(QString());
            QMetaObject::invokeMethod(tapEdit, "textEdited", Qt::DirectConnection, Q_ARG(QString, QString()));
            check(unavailableAt(16, 23), "Incomplete visible tap text must not reuse the hidden source value");
            visibleStep->setValue(3.0);
            check(linkedGrid->item(16, 23)->text().toDouble() > 0.0,
                  "Valid visible tap input must recover winding preview");
            visibleStep->setValue(2.5);
        }
        linkedGrid->item(11, 7)->setText(QStringLiteral("bad")); // H12
        check(unavailableAt(16, 6) && unavailableAt(16, 23),
              "Invalid geometry text must clear all partial outputs rather than use old input");
        linkedGrid->item(11, 7)->setText(QStringLiteral("170"));
        check(numberAt(16, 6, 165.0), "Valid geometry must recover without rebuilding the popup");
        QString previewError;
        check(!linked.inputsReady(previewError), "Partial results must never enable whole-machine confirmation");
        linkedShape->setCurrentIndex(2);
        linked.refreshResult(detailed);
        check(linkedGrid->item(17, 5)->text() == QStringLiteral("0 字")
                  && numberAt(50, 13, 38.0) && linked.partialPreviewStatus().isEmpty(),
              "Returning to ellipse must restore full results and remove long-round header values");
        const QString layoutPath = qEnvironmentVariable("ZTBLD_SHEET_LAYOUT_IMAGE");
        if (!layoutPath.isEmpty()) {
            linked.resize(2300,2200);linked.show();app.processEvents();
            check(linked.grab().save(layoutPath),"Capture original sheet layout");
            linked.hide();
        }
        const QString previewPath = qEnvironmentVariable("ZTBLD_PARTIAL_PREVIEW_IMAGE");
        if (!previewPath.isEmpty()) {
            linkedShape->setCurrentIndex(1);
            linked.resize(1900, 1050);
            linked.show();
            app.processEvents();
            check(linked.grab().save(previewPath), "Could not capture offscreen preview for visual verification");
            linked.hide();
        }
    }
    CalcInput roundBranch;
    roundBranch.coreShape = StructureConfig::LongRound;
    const auto partial = ElectromagneticEngine::previewShapeBranch(roundBranch);
    const auto closeValue = [&](const ShapeBranchPreview &r, const char *key, double expected) {
        const auto it = r.values.constFind(QLatin1String(key));
        return it != r.values.cend() && std::abs(it->first - expected) < 0.0000001;
    };
    // Hand-derived from the original formulas: LV radial28.5, HV radial39,
    // AB36=ROUND(pi*168,2), AK33=ROUND(pi*111,2), K12=103.
    check(partial.geometryAvailable && partial.windingAvailable
              && closeValue(partial, "AB36", 527.79) && closeValue(partial, "AK33", 348.72)
              && closeValue(partial, "X17", 1.26158) && closeValue(partial, "AH17", .90344)
              && closeValue(partial, "Z18", 983.3) && closeValue(partial, "X19", 1.916685)
              && closeValue(partial, "AH19", .000850) && closeValue(partial, "Y20", 2536.0)
              && closeValue(partial, "AH20", 2109.0),
          "Long-round winding chain must agree with independently derived original-formula values");
    for (const char *key : {"F14", "F16", "N14", "L10", "Q35", "AC47", "N51"})
        check(!partial.values.contains(QLatin1String(key)), "Partial preview must not emit unverified whole-machine values");
    roundBranch.coreShape = StructureConfig::Circle;
    const auto circularPending = ElectromagneticEngine::previewShapeBranch(roundBranch);
    check(circularPending.geometryAvailable && !circularPending.windingAvailable
              && !circularPending.values.contains(QStringLiteral("X17")) && roundBranch.coreStraight_mm == 103.0,
          "Circle must preserve nonzero K12 and withhold dependent winding outputs");
    roundBranch.coreStraight_mm = 0.0;
    roundBranch.ellipseAngle_deg = std::numeric_limits<double>::quiet_NaN();
    const auto circular = ElectromagneticEngine::previewShapeBranch(roundBranch);
    check(circular.windingAvailable && closeValue(circular, "X17", 1.05558)
              && closeValue(circular, "AH17", .69744),
          "Confirmed circular K12=0 must use circle arcs independently of ellipse angle");
    roundBranch.coreDiameter_mm = 1.0;
    const auto invalidPreview = ElectromagneticEngine::previewShapeBranch(roundBranch);
    check(!invalidPreview.geometryAvailable && !invalidPreview.windingAvailable && invalidPreview.values.isEmpty(),
          "Out-of-table shape must not retain any previous preview values");
    CalcInput roundWireInput;
    roundWireInput.coreShape = StructureConfig::LongRound;
    roundWireInput.hvBareWidth_mm = roundWireInput.hvBareThick_mm = 2.0;
    StructureConfig roundWireConfig;
    roundWireConfig.coreShape = StructureConfig::LongRound;
    ParamTableWidget roundWireTable;
    roundWireTable.loadParamsForConfig(TransformerParams{}, roundWireConfig, roundWireInput, true);
    QComboBox *roundSpec = nullptr;
    for (int row = 0; row < roundWireTable.rowCount(); ++row)
        if (roundWireTable.item(row, 3) && roundWireTable.item(row, 3)->text() == QStringLiteral("圆线裸直径(mm)"))
            roundSpec = qobject_cast<QComboBox *>(roundWireTable.cellWidget(row, 4));
    check(roundSpec != nullptr, "Missing round wire specification selector");
    if (roundSpec) {
        QString error;
        check(roundWireTable.collectShapePreviewInput(roundWireInput, error), "Valid round wire must collect for preview");
        roundSpec->setCurrentIndex(-1);
        check(!roundWireTable.collectShapePreviewInput(roundWireInput, error),
              "Missing round-wire selection must not reuse an earlier valid diameter");
    }
    qInfo() << "Core selection checks failed:" << failures;
    return failures == 0 ? 0 : 1;
}
