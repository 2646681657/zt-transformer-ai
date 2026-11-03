#include "ParamTableWidget.h"
#include "ModelStdTable.h"
#include "DesignDatabase.h"
#include "CalculationApplicability.h"
#include "ElectromagneticEngine.h"
#include "TestVoltageHints.h"
#include "HvCoilFormNotes.h"
#include "CoreLaminationPreview.h"
#include <QHeaderView>
#include <QFont>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace {
enum class SupportedConnection { Invalid, Dyn11, Yyn0, Dd, Yd };

SupportedConnection connectionType(QString value)
{
    value.remove(QRegularExpression(QStringLiteral("[\\s,/]+")));
    if (value.compare(QLatin1String("Dyn"), Qt::CaseInsensitive) == 0 ||
        value.compare(QLatin1String("Dyn11"), Qt::CaseInsensitive) == 0) {
        return SupportedConnection::Dyn11;
    }
    if (value.compare(QLatin1String("Yyn"), Qt::CaseInsensitive) == 0 ||
        value.compare(QLatin1String("Yyn0"), Qt::CaseInsensitive) == 0) {
        return SupportedConnection::Yyn0;
    }
    // Exact legacy labels are produced when opening an existing input snapshot.
    if (value.compare(QLatin1String("Dd"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("Dd（暂不支持）"), Qt::CaseInsensitive) == 0)
        return SupportedConnection::Dd;
    if (value.compare(QLatin1String("Yd"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("Yd（暂不支持）"), Qt::CaseInsensitive) == 0)
        return SupportedConnection::Yd;
    return SupportedConnection::Invalid;
}

bool hasBuiltInConnectionStandards(SupportedConnection type)
{
    return type == SupportedConnection::Dyn11 || type == SupportedConnection::Yyn0;
}
}

ParamTableWidget::ParamTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupTable();
    m_corePreviewTimer = new QTimer(this);
    m_corePreviewTimer->setSingleShot(true);
    m_corePreviewTimer->setInterval(200);
    connect(m_corePreviewTimer, &QTimer::timeout, this, &ParamTableWidget::updateDesignCore);
    connect(this, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *changed) {
        if (m_loading)
            return;
        const auto currentRef = m_inputRefs.constFind(QStringLiteral("noLoadCurrentStd"));
        if (!m_updatingNonstandardAn3
                && currentRef != m_inputRefs.cend() && changed->row() == currentRef->first && changed->column() == currentRef->second) {
            bool valid = false;
            const double current = changed->text().toDouble(&valid);
            m_baseParams.noLoadCurrentSource=TransformerParams::NoLoadCurrentSource::Manual;
            if(m_currentSourceCombo){const QSignalBlocker blocker(m_currentSourceCombo);m_currentSourceCombo->setCurrentIndex(1);}
            m_baseParams.hasNonstandardAn3 = false;
            if (valid && std::isfinite(current) && current >= 0.0) {
                m_baseParams.hasSavedCustomNoLoadCurrent = true;
                m_baseParams.savedCustomNoLoadCurrent_pct = current;
            }
            applyModelLinkage();
            emit nonstandardAn3StateChanged();
        }
        for (const QString &key : {QStringLiteral("connectionGroup"), QStringLiteral("frequency"),
                                  QStringLiteral("noLoadLossStd"), QStringLiteral("loadLossStd"),
                                  QStringLiteral("totalLossStd")}) {
            const auto it = m_inputRefs.constFind(key);
            if (it == m_inputRefs.constEnd() || changed->row() != it->first || changed->column() != it->second)
                continue;
            if (key.endsWith(QStringLiteral("LossStd"))) {
                m_lossStandardsManual = true;
                m_lastLinkageKey = standardKey();
                // 手工损耗覆盖不改变原单国标/非标标识。
            }
            applyModelLinkage();
            break;
        }
        updateYokePiece1();
        updateLvTurnsRecommendation();
        updateDesignConnection();
        scheduleDesignCore();
    });
}

void ParamTableWidget::setupTable()
{
    setColumnCount(6);
    setHorizontalHeaderLabels({"#", "参数名称", "数值", "选项名称", "选项", "备注"});
    horizontalHeader()->setStretchLastSection(true);
    setColumnWidth(0, 40);
    setColumnWidth(1, 160);
    setColumnWidth(2, 125);
    setColumnWidth(3, 180);
    setColumnWidth(4, 120);
    verticalHeader()->setVisible(false);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
}

void ParamTableWidget::addSectionRow(int row, const QString &title,
                                      const QString &optName, const QString &optValue,
                                      bool advanced)
{
    insertRow(row);
    auto *item = new QTableWidgetItem(title);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
    // 高级节（七~十）琥珀色标题，普通节（一~六）保持青蓝色
    item->setBackground(advanced ? QColor("#FFF2D2") : QColor("#DCEEDF"));
    item->setForeground(advanced ? QColor("#8A5900") : QColor("#24362B"));
    auto *numItem = new QTableWidgetItem(QString::number(row + 1));
    numItem->setTextAlignment(Qt::AlignCenter);
    numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 0, numItem);
    setItem(row, 1, item);
    auto *emptyValueItem = new QTableWidgetItem("");
    emptyValueItem->setFlags(emptyValueItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 2, emptyValueItem);
    auto *optNameItem = new QTableWidgetItem(optName);
    optNameItem->setFlags(optNameItem->flags() & ~Qt::ItemIsEditable);
    optNameItem->setBackground(advanced ? QColor("#FFF2D2") : QColor("#DCEEDF"));
    setItem(row, 3, optNameItem);
    auto *optValItem = new QTableWidgetItem(optValue);
    optValItem->setFlags(optValItem->flags() & ~Qt::ItemIsEditable);
    optValItem->setBackground(advanced ? QColor("#FFF2D2") : QColor("#DCEEDF"));
    setItem(row, 4, optValItem);
    auto *noteItem = new QTableWidgetItem("");
    noteItem->setFlags(noteItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 5, noteItem);
}

void ParamTableWidget::addParamRow(int row, const QString &name, const QString &value,
                                    const QString &optName, const QString &optValue)
{
    insertRow(row);
    auto *numItem = new QTableWidgetItem(QString::number(row + 1));
    numItem->setTextAlignment(Qt::AlignCenter);
    numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 0, numItem);
    auto *nameItem = new QTableWidgetItem(name);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 1, nameItem);
    auto *valItem = new QTableWidgetItem(value);
    setItem(row, 2, valItem);
    auto *optNameItem = new QTableWidgetItem(optName);
    optNameItem->setFlags(optNameItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 3, optNameItem);
    auto *optItem = new QTableWidgetItem(optValue);
    if (optName.isEmpty() && optValue.isEmpty()) {
        optItem->setFlags(optItem->flags() & ~Qt::ItemIsEditable);
    }
    setItem(row, 4, optItem);
    auto *noteItem = new QTableWidgetItem("");
    noteItem->setFlags(noteItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 5, noteItem);
}

void ParamTableWidget::bindInput(const QString &key, int row, int col)
{
    if (!key.isEmpty()) {
        m_inputRefs.insert(key, { row, col });
        // 部分数值（如阻抗最小偏差）位于通常展示选项名称的列。
        if (auto *valueItem = item(row, col)) {
            valueItem->setFlags(valueItem->flags() | Qt::ItemIsEditable);
        }
    }
}

void ParamTableWidget::addInputRow(int row, const QString &name, const QString &value,
                                   const QString &optName, const QString &optValue,
                                   const QString &key, const QString &optKey)
{
    addParamRow(row, name, value, optName, optValue);
    bindInput(key, row, 2);
    bindInput(optKey, row, 4);
}

bool ParamTableWidget::setNonstandardAn3(double value)
{
    if (m_standardMode != TransformerParams::StandardMode::Custom || m_baseParams.hasManualNoLoadCurrent() || !std::isfinite(value) || value <= 0.34) return false;
    const auto ref = m_inputRefs.constFind(QStringLiteral("noLoadCurrentStd"));
    if (ref == m_inputRefs.cend() || !item(ref->first, ref->second)) return false;
    const double converted = QString::number(value - 0.34, 'g', 12).toDouble();
    m_baseParams.hasNonstandardAn3 = true;
    m_baseParams.nonstandardAn3_pct = value;
    m_baseParams.hasSavedCustomNoLoadCurrent = true;
    m_baseParams.savedCustomNoLoadCurrent_pct = converted;
    m_updatingNonstandardAn3 = true;
    item(ref->first, ref->second)->setText(QString::number(converted, 'g', 12));
    m_updatingNonstandardAn3 = false;
    applyModelLinkage();emit nonstandardAn3StateChanged();
    return true;
}

TransformerParams ParamTableWidget::getParams() const
{
    TransformerParams params = m_baseParams;
    // 静态节（输入信息/性能指标）也用 key 绑定读取，与行号解耦
    const auto cellText = [this](const QString &key) -> QString {
        const auto it = m_inputRefs.constFind(key);
        if (it == m_inputRefs.constEnd() || !item(it->first, it->second)) {
            return QString();
        }
        return item(it->first, it->second)->text().trimmed();
    };
    const auto setDouble = [&cellText](const QString &key, double &dst) {
        bool ok = false;
        const double v = cellText(key).toDouble(&ok);
        if (ok) {
            dst = v;
        }
    };
    const auto setInt = [&cellText](const QString &key, int &dst) {
        bool ok = false;
        const int v = cellText(key).toInt(&ok);
        if (ok) {
            dst = v;
        }
    };
    const auto setString = [&cellText](const QString &key, QString &dst) {
        const QString s = cellText(key);
        if (!s.isEmpty()) {
            dst = s;
        }
    };

    // 容量及高/低压由复合产品型号增量解析后保存。
    params.capacity_kVA = m_modelCapacity_kVA;
    params.hvRatedVoltage_kV = m_modelHvRated_kV;
    params.lvRatedVoltage_kV = m_modelLvRated_kV;
    if (m_tapPlusSpin && m_tapMinusSpin && m_tapStepSpin) {
        params.hvTapPlusSteps = m_tapPlusSpin->value();
        params.hvTapMinusSteps = m_tapMinusSpin->value();
        params.hvTapVoltagePercent = m_tapStepSpin->value();
    }
    setString("environmentGrade", params.environmentGrade);
    setDouble("calcRefTemp", params.calcRefTemp_C);
    setDouble("maxAmbientTemp", params.maxAmbientTemp_C);
    setDouble("maxAltitude", params.maxAltitude_m);
    setString("efficiencyCalcMethod", params.efficiencyCalcMethod);
    if (m_productModelEdit && !m_productModelEdit->text().trimmed().isEmpty()) {
        params.productModel = m_productModelEdit->text().trimmed();
    }
    setString("connectionGroup", params.connectionGroup);
    setInt("frequency", params.frequency_Hz);
    params.lossStandardsManual = m_lossStandardsManual;
    params.standardMode = m_standardMode;
    params.lossStandardsKey = m_lastLinkageKey;

    // 性能指标
    setDouble("noLoadLossStd", params.noLoadLossStd_W);
    setDouble("noLoadLossMaxDev", params.noLoadLossMaxDev_pct);
    setDouble("loadLossStd", params.loadLossStd_W);
    setDouble("loadLossMaxDev", params.loadLossMaxDev_pct);
    setDouble("totalLossStd", params.totalLossStd_W);
    setDouble("totalLossMaxDev", params.totalLossMaxDev_pct);
    setDouble("impedanceVoltageStd", params.impedanceVoltageStd_pct);
    setDouble("impedanceVoltageMinDev", params.impedanceVoltageMinDev_pct);
    setDouble("impedanceVoltageMaxDev", params.impedanceVoltageMaxDev_pct);
    setDouble("noLoadCurrentStd", params.noLoadCurrentStd_pct);
    setDouble("noLoadCurrentMaxDev", params.noLoadCurrentMaxDev_pct);

    // 温升限值
    setDouble("oilTopTempRise", params.oilTopTempRise_K);
    setDouble("hvCoilTempRise", params.hvCoilTempRise_K);
    setDouble("lvCoilTempRise", params.lvCoilTempRise_K);

    return params;
}

TransformerParams ParamTableWidget::paramsForInput(const TransformerParams &base, const CalcInput &input)
{
    TransformerParams params = base;
    if (input.hasPerformanceCriteria && PerformanceCriteria::validModel(input.performanceCriteria.productModel)) {
        PerformanceCriteria::restore(input.performanceCriteria, params);
    } else {
        params.standardMode = TransformerParams::StandardMode::Unconfirmed;
        params.lossStandardsManual = true;
        params.lossStandardsKey.clear();
        // Missing provenance must not borrow raw inputs/caches from the last scheme.
        params.hasNonstandardAn3 = false;
        params.nonstandardAn3_pct = 0.0;
        params.hasSavedCustomNoLoadCurrent = false;
        params.savedCustomNoLoadCurrent_pct = 0.0;
    }
    params.capacity_kVA = input.capacity_kVA;
    params.hvRatedVoltage_kV = input.hvRated_kV;
    params.lvRatedVoltage_kV = input.lvRated_kV;
    params.hvTapPlusSteps = input.hvTapPlusSteps;
    params.hvTapMinusSteps = input.hvTapMinusSteps;
    params.hvTapVoltagePercent = input.hvTapStep_pct;
    params.connectionGroup = input.lvStarConnected
        ? (input.hvDeltaConnected ? QStringLiteral("Dyn11") : QStringLiteral("Yyn0"))
        : (input.hvDeltaConnected ? QStringLiteral("Dd") : QStringLiteral("Yd"));
    static const QRegularExpression seriesPattern(QStringLiteral("^(.+-M)(?:-|$)"),
                                                  QRegularExpression::CaseInsensitiveOption);
    const auto match = seriesPattern.match(params.productModel.trimmed());
    const QString series = match.hasMatch() ? match.captured(1) : QStringLiteral("SB20-M");
    params.productModel = QStringLiteral("%1-%2/%3-%4")
        .arg(series, QString::number(input.capacity_kVA, 'g', 12),
             QString::number(input.hvRated_kV, 'g', 12), QString::number(input.lvRated_kV, 'g', 12));
    return params;
}

bool ParamTableWidget::collectShapePreviewInput(CalcInput &input, QString &error, bool validateLossInputs) const
{
    error.clear();
    const auto config = calculationConfig();
    if (config.category != StructureConfig::OilImmersed || config.windingProcess != StructureConfig::FoilWound
            || config.coreType != StructureConfig::StackedSilicon || config.windingForm != StructureConfig::Dual
            || (config.hvCoilStructure != StructureConfig::MultiLayerCylinder
                && !(validateLossInputs && config.hvCoilStructure == StructureConfig::TwoSegCylinder))) {
        error = QStringLiteral("本批绕组部分联动仅用于油浸箔绕、叠铁芯、双绕组、多层圆筒式。");
        return false;
    }
    const auto params = getParams();
    if (params.frequency_Hz != 50 || params.calcRefTemp_C != 75.0) {
        error = QStringLiteral("本批绕组部分联动固定50Hz、75℃，不套用其他频率或折算温度。");
        return false;
    }
    static const QRegularExpression modelPattern(QStringLiteral(
        "^[A-Za-z0-9]+-M-([0-9]+(?:\\.[0-9]+)?)/([0-9]+(?:\\.[0-9]+)?)-([0-9]+(?:\\.[0-9]+)?)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto model = modelPattern.match(m_productModelEdit ? m_productModelEdit->text().trimmed() : QString());
    if (!model.hasMatch()) { error = QStringLiteral("型号输入未完整，部分联动不使用旧额定值。"); return false; }
    QStringList keys = {"coreDiameter", "coreStraight", "phaseGapBase", "hvBareWidth", "hvBareThick",
        "hvTurnsPerLayer", "hvLayerInsul", "hvParallelCount", "hvStackCount", "lvTurns", "lvFoilThick",
        "lvFoilWidth", "lvLayerInsulCount", "lvLayerInsul_mm", "lvEndInsul", "mainDuctWidth", "mainDuctInsul",
        "frequency", "calcRefTemp", "centerDistanceAllowance", "hvLayerInsulCalcFactor", "hvMaxTurnsAdjust", "activePartHvWeightMode", "hvAxialAllowance", "hvDuctLayerRule", "impedanceOutputMode", "impedanceCalibrationFactor",
        "tankRimHighCapacity", "tankWallThicknessOverride", "tankBottomThicknessAdjust", "conservatorWeight", "waveLongCountAdjust"};
    if (validateLossInputs) keys << QStringLiteral("strayLossFactor") << QStringLiteral("lvExtraLoss")
        << QStringLiteral("stackFactor") << QStringLiteral("ellipseAngle") << QStringLiteral("seamCount")
        << QStringLiteral("coreLossCraftCoef") << QStringLiteral("yokeLossCraftCoef")
        << QStringLiteral("thermalFormulaMode") << QStringLiteral("lvThermalExtraMode")
        << QStringLiteral("designAreaMode")
        << QStringLiteral("unitIronLossMode") << QStringLiteral("manualCoreLossPerKg") << QStringLiteral("manualYokeLossPerKg")
        << QStringLiteral("yokePiece1Stack") << QStringLiteral("yokePiece2Stack") << QStringLiteral("yokePiece3Stack")
        << QStringLiteral("yokeWidenTo") << QStringLiteral("yokeWidenStages")
        << QStringLiteral("tankBottomOil") << QStringLiteral("tankSideClear") << QStringLiteral("tankEndClear")
        << QStringLiteral("tankFoot") << QStringLiteral("waveDepth") << QStringLiteral("waveHeight")
        << QStringLiteral("wavePitch") << QStringLiteral("oilTopCorrection");
    if (validateLossInputs)
        for (int i=0;i<20;++i) keys << QStringLiteral("coreSheetAdjust_%1").arg(i);
    for(int i=0;i<20;++i)keys << QStringLiteral("coreWidthOffset_%1").arg(i);
    if (!m_roundWireActive) keys << QStringLiteral("hvWireInsulAdd");
    for (int i = 0; i < 5; ++i)
        for (const char *prefix : {"hvDuctW_", "hvDuctH_", "lvDuctW_", "lvDuctH_"})
            keys << QString::fromLatin1(prefix) + QString::number(i);
    const QStringList integerKeys = {"hvTurnsPerLayer", "hvParallelCount", "hvStackCount", "lvTurns",
                                     "lvLayerInsulCount", "frequency", "seamCount", "yokeWidenStages",
                                     "waveDepth", "waveHeight", "wavePitch"};
    for (const auto &key : keys) {
        const auto it = m_inputRefs.constFind(key);
        if (it == m_inputRefs.cend()) continue; // unexposed advanced input retains its saved value
        const auto *source = item(it->first, it->second);
        bool ok = false;
        const QString text = source ? source->text().trimmed() : QString();
        const double value = text.toDouble(&ok);
        if (ok && (integerKeys.contains(key) || key.startsWith(QStringLiteral("coreSheetAdjust_")))) text.toInt(&ok);
        if (!ok || !std::isfinite(value)) {
            const auto *label = item(it->first, it->second == 2 ? 1 : 3);
            error = QStringLiteral("%1输入未完整或无效；不沿用旧值。")
                .arg(label ? label->text() : key);
            return false;
        }
    }
    for (auto *spin : {static_cast<QAbstractSpinBox *>(m_tapPlusSpin),
                        static_cast<QAbstractSpinBox *>(m_tapMinusSpin),
                        static_cast<QAbstractSpinBox *>(m_tapStepSpin)}) {
        if (spin && !spin->hasAcceptableInput()) { error = QStringLiteral("调压输入未完整。"); return false; }
    }
    if (!connectionCalculationError().isEmpty()) { error = connectionCalculationError(); return false; }
    if (m_roundWireActive) {
        WireSpec spec;
        if (!m_roundWireSpecCombo || !m_roundWireSpecCombo->currentData().isValid()
                || !DesignDatabase::instance().roundWireSpec(m_roundWireSpecCombo->currentData().toDouble(), spec)) {
            error = QStringLiteral("请选择有效圆线表内规格；部分联动不沿用旧规格。");
            return false;
        }
    } else {
        const auto w = m_inputRefs.value(QStringLiteral("hvBareWidth"));
        const auto t = m_inputRefs.value(QStringLiteral("hvBareThick"));
        if (item(w.first, w.second)->text().toDouble() == item(t.first, t.second)->text().toDouble()) {
            error = QStringLiteral("宽厚相等时需明确选择圆线表规格，不能按扁线预览。");
            return false;
        }
    }
    CalcInput candidate = input;
    saveToInput(candidate);
    candidate.capacity_kVA = model.captured(1).toDouble();
    candidate.hvRated_kV = model.captured(2).toDouble();
    candidate.lvRated_kV = model.captured(3).toDouble();
    const auto widthError=candidate.coreWidthOffsetError();
    if(!widthError.isEmpty()){error=widthError;return false;}
    const auto processError=candidate.coreWeightProcessInputError();
    if(!processError.isEmpty()){error=processError;return false;}
    const auto areaError=candidate.designAreaInputError();if(!areaError.isEmpty()){error=areaError;return false;}
    const auto thermalError=candidate.thermalProcessInputError();if(!thermalError.isEmpty()){error=thermalError;return false;}
    const auto unitLossError=candidate.coreUnitLossInputError();if(!unitLossError.isEmpty()){error=unitLossError;return false;}
    const auto electricalError=candidate.electricalProcessInputError();if(!electricalError.isEmpty()){error=electricalError;return false;}
    const auto tankError=candidate.tankProcessInputError();if(!tankError.isEmpty()){error=tankError;return false;}
    input = candidate;
    return true;
}

bool ParamTableWidget::collectForCalculation(TransformerParams &params, CalcInput &input, QString &error,
                                            bool interactive)
{
    error.clear();
    const auto currentParams=getParams();
    if(!currentParams.hasManualNoLoadCurrent()){
        double ignored=0;
        if(m_standardMode==TransformerParams::StandardMode::Custom&&!currentParams.hasNonstandardAn3){error=QStringLiteral("空载电流选择自动联动时，非标模式必须输入有效AN3；也可切换为手工固定。");return false;}
        if(m_standardMode==TransformerParams::StandardMode::BuiltIn&&(!standardUnavailableReason().isEmpty()||!ModelStdTable::originalNoLoadCurrent(m_modelCapacity_kVA,ignored))){error=QStringLiteral("空载电流自动来源未匹配，请核对规格或选择手工固定。");return false;}
    }
    if (m_standardMode == TransformerParams::StandardMode::Unconfirmed) {
        error = QStringLiteral("此方案未保存完整指标来源。请在设计输入页选择内置标准或非标，并核对性能指标后再计算。");
        return false;
    }
    // 增量输入只负责编辑期间联动；计算/确认时必须完整解析屏幕上的型号。
    static const QRegularExpression modelPattern(
        QStringLiteral("^([A-Za-z0-9]+-M)-([0-9]+(?:\\.[0-9]+)?)/([0-9]+(?:\\.[0-9]+)?)-([0-9]+(?:\\.[0-9]+)?)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = modelPattern.match(m_productModelEdit ? m_productModelEdit->text().trimmed() : QString());
    if (!match.hasMatch()) {
        error = QStringLiteral("产品型号不完整或格式错误，请按“SB20-M-630/10-0.4”输入；小数只使用点号，不支持逗号。");
        return false;
    }
    double ratings[3];
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        ratings[i] = match.captured(i + 2).toDouble(&ok);
        if (!ok || !std::isfinite(ratings[i]) || ratings[i] <= 0.0) {
            error = QStringLiteral("产品型号中的容量、高压和低压额定电压必须是大于零的有效数值。");
            return false;
        }
    }
    const QString connectionError = connectionCalculationError();
    if (!connectionError.isEmpty()) {
        error = connectionError;
        return false;
    }
    if (!hasValidSteelGrade()) {
        error = QStringLiteral("请选择数据库已收录的硅钢片牌号，未收录牌号不能用于计算。");
        return false;
    }
    if (m_wireFormCombo && m_wireFormCombo->currentData().toBool()) {
        WireSpec spec;
        if (!m_roundWireSpecCombo || !m_roundWireSpecCombo->currentData().isValid()
                || !DesignDatabase::instance().roundWireSpec(m_roundWireSpecCombo->currentData().toDouble(), spec)) {
            error = QStringLiteral("请选择有效圆线表内规格；未收录规格、缺失或无效表数据不能用于计算。");
            return false;
        }
    } else {
        const auto width = m_inputRefs.value(QStringLiteral("hvBareWidth"));
        const auto thick = m_inputRefs.value(QStringLiteral("hvBareThick"));
        bool widthOk = false, thickOk = false;
        const double w = item(width.first, width.second)->text().toDouble(&widthOk);
        const double t = item(thick.first, thick.second)->text().toDouble(&thickOk);
        if (widthOk && thickOk && w == t) {
            error = QStringLiteral("宽=厚按计算单属于圆线，请选择“圆线（计算单表）”并从下拉框选择规格。");
            return false;
        }
    }
    const QStringList textKeys = {"noLoadCurrentSource", "connectionGroup", "environmentGrade", "efficiencyCalcMethod",
        "lvConnectionBarThickness", "lvConnectionBarWidth", "legacyUnitLossRecord", "legacyCraftCoefRecord"}; // 可选记录不阻断整机。
    const QStringList integerKeys = {"frequency", "seamCount", "lvTurns", "hvTurnsPerLayer", "hvParallelCount",
        "hvStackCount", "lvLayerInsulCount", "yokeWidenStages", "waveDepth", "waveHeight", "wavePitch"};
    for (auto it = m_inputRefs.constBegin(); it != m_inputRefs.constEnd(); ++it) {
        if (textKeys.contains(it.key()))
            continue;
        const auto *cell = item(it->first, it->second);
        bool ok = false;
        const QString value = cell ? cell->text().trimmed() : QString();
        const double number = value.toDouble(&ok);
        if (ok && (integerKeys.contains(it.key()) || it.key().startsWith(QStringLiteral("coreSheetAdjust_"))))
            value.toInt(&ok);
        if (!ok || !std::isfinite(number)) {
            const auto *label = item(it->first, it->second == 2 ? 1 : 3);
            error = QStringLiteral("“%1”请输入有效%2，不能留空或输入非数字。")
                .arg(label && !label->text().isEmpty() ? label->text() : it.key(),
                     (integerKeys.contains(it.key()) || it.key().startsWith(QStringLiteral("coreSheetAdjust_"))) ? QStringLiteral("整数") : QStringLiteral("数值"));
            return false;
        }
    }
    if (m_tapMinusSpin->value() * m_tapStepSpin->value() >= 100.0) {
        error = QStringLiteral("负向调压总幅度必须小于100%，最低分接电压必须大于零。");
        return false;
    }
    m_modelSeries = match.captured(1);
    m_modelCapacity_kVA = ratings[0];
    m_modelHvRated_kV = ratings[1];
    m_modelLvRated_kV = ratings[2];
    applyModelLinkage();
    TransformerParams collectedParams = getParams();
    CalcInput collectedInput = input;
    saveToInput(collectedInput);
    collectedInput.capacity_kVA = collectedParams.capacity_kVA;
    collectedInput.hvRated_kV = collectedParams.hvRatedVoltage_kV;
    collectedInput.lvRated_kV = collectedParams.lvRatedVoltage_kV;
    const QString coreAdjustmentError=collectedInput.coreSheetCountAdjustmentError();
    if (!coreAdjustmentError.isEmpty()) { error=coreAdjustmentError; return false; }
    const auto coreWidthError=collectedInput.coreWidthOffsetError();
    if(!coreWidthError.isEmpty()){error=coreWidthError;return false;}
    const auto processError=collectedInput.coreWeightProcessInputError();
    if(!processError.isEmpty()){error=processError;return false;}
    const auto areaError=collectedInput.designAreaInputError();if(!areaError.isEmpty()){error=areaError;return false;}
    const auto thermalError=collectedInput.thermalProcessInputError();if(!thermalError.isEmpty()){error=thermalError;return false;}
    const auto unitLossError=collectedInput.coreUnitLossInputError();if(!unitLossError.isEmpty()){error=unitLossError;return false;}
    const auto electricalError=collectedInput.electricalProcessInputError();if(!electricalError.isEmpty()){error=electricalError;return false;}
    const auto tankError=collectedInput.tankProcessInputError();if(!tankError.isEmpty()){error=tankError;return false;}
    const QString ductError = collectedInput.oilDuctInputError();
    if (!ductError.isEmpty()) {
        error = ductError;
        return false;
    }
    if (collectedInput.coreLossCraftCoef <= 0.0 || collectedInput.yokeLossCraftCoef <= 0.0) {
        error = QStringLiteral("心柱和铁轭铁损工艺系数必须是大于零的有效数值。");
        return false;
    }
    if (collectedInput.lvTurns <= 0 || collectedInput.hvTurnsPerLayer < 2 ||
        collectedInput.hvParallelCount <= 0 || collectedInput.hvStackCount <= 0 ||
        collectedInput.seamCount <= 0) {
        error = QStringLiteral("低压匝数、并绕/叠绕根数、接缝数必须大于零，高压总层数 W12 必须至少为2。");
        return false;
    }
    const QString scopeError = calculationScopeError(m_config, collectedParams, collectedInput);
    if (!scopeError.isEmpty()) {
        error = scopeError + QStringLiteral("。请核对油浸式、低压箔绕、叠铁芯、椭圆形、双绕组及线圈型式，并使用50Hz/75℃配置。");
        return false;
    }
    const QString unavailable = m_standardMode == TransformerParams::StandardMode::BuiltIn
        ? standardUnavailableReason() : QString();
    if (!unavailable.isEmpty() && !interactive) {
        error = unavailable + QStringLiteral("；请核对损耗标准后点击确认。自动预览不代替人工核对。");
        return false;
    }
    if (!unavailable.isEmpty() && QMessageBox::question(this, QStringLiteral("损耗标准需手工核对"),
        unavailable + QStringLiteral("\n当前保留的空载/负载/总损耗标准为 %1/%2/%3 W，不代表此规格已匹配标准表。\n是否已核对这些手工标准值并继续计算？")
            .arg(collectedParams.noLoadLossStd_W).arg(collectedParams.loadLossStd_W).arg(collectedParams.totalLossStd_W),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        error = QStringLiteral("请先核对或修改性能指标中的损耗标准值，再继续计算。");
        return false;
    }
    params = collectedParams;
    input = collectedInput;
    return true;
}

bool ParamTableWidget::hasSupportedConnectionGroup() const
{
    return connectionCalculationError().isEmpty();
}

QString ParamTableWidget::connectionCalculationError() const
{
    const auto it = m_inputRefs.constFind(QStringLiteral("connectionGroup"));
    const auto type = it != m_inputRefs.constEnd() && item(it->first, it->second)
        ? connectionType(item(it->first, it->second)->text()) : SupportedConnection::Invalid;
    if (type == SupportedConnection::Invalid)
        return QStringLiteral("联结组别无法识别；当前支持Dyn11（可填Dyn）、Yyn0，以及仅限非标手工指标的Dd、Yd。");
    if (!hasBuiltInConnectionStandards(type) && m_standardMode != TransformerParams::StandardMode::Custom)
        return QStringLiteral("Dd、Yd无对应内置损耗标准列。请明确选择“非标（手工指标）”并核对损耗、阻抗、空载电流及温升指标后再计算或保存；不能借用Dyn标准。");
    return {};
}

bool ParamTableWidget::hasValidSteelGrade() const
{
    const QString grade = selectedSteelGrade();
    const DesignDatabase &db = DesignDatabase::instance();
    return db.isLoaded() && db.steelGradeExists(grade) &&
           CalcInput::thicknessFromSteelGrade(grade) > 0.0;
}

QString ParamTableWidget::selectedSteelGrade() const
{
    return m_steelGradeCombo ? m_steelGradeCombo->currentData().toString().trimmed() : QString();
}

void ParamTableWidget::updateSteelThickness()
{
    const auto thickness = m_inputRefs.constFind(QStringLiteral("steelThickness"));
    if (thickness == m_inputRefs.constEnd()) {
        return;
    }
    auto *thicknessItem = item(thickness->first, thickness->second);
    if (!thicknessItem) {
        return;
    }
    const double value = hasValidSteelGrade()
        ? CalcInput::thicknessFromSteelGrade(selectedSteelGrade()) : 0.0;
    const QSignalBlocker blocker(this);
    thicknessItem->setText(value > 0.0 ? QString::number(value) : QString());
    if (m_steelCurveRangeRow >= 0) {
        double minT = 0.0, maxT = 0.0;
        const bool available = DesignDatabase::instance().steelCurveRange(selectedSteelGrade(), minT, maxT);
        item(m_steelCurveRangeRow, 2)->setText(available ? QString::number(minT, 'g', 12) : QStringLiteral("无有效数据"));
        item(m_steelCurveRangeRow, 4)->setText(available ? QString::number(maxT, 'g', 12) : QStringLiteral("无有效数据"));
        item(m_steelCurveRangeRow, 5)->setText(QStringLiteral("含端点；越界禁止计算"));
        const QString note = QStringLiteral("内置牌号曲线的数据范围，不是材料保证值。按实际查表磁密检查心柱铁损、铁轭铁损及磁化容量；不外推、不取端点替代。");
        item(m_steelCurveRangeRow, 2)->setToolTip(note);
        item(m_steelCurveRangeRow, 4)->setToolTip(note);
        item(m_steelCurveRangeRow, 5)->setToolTip(note);
    }
    updateYokePiece1();
    updateLvTurnsRecommendation();
    scheduleDesignCore();
}

// 型号/容量联动：查 GB 20052-2024 叠铁芯标准值，覆盖空载/负载/总损耗标准值单元格，
// 并发出摘要信号；联结组别为 Yyn0 时负载损耗取 Yyn0 列，否则取 Dyn11/Yzn11 列。
// 阻抗电压为产品铭牌参数（跟随具体计算单/订单，不随能效等级变化），不参与联动覆盖
bool ParamTableWidget::parseCompositeModel(const QString &text, bool commitLowVoltage)
{
    const QString value = text.trimmed();
    const int markerPos = value.indexOf(QStringLiteral("-M-"), 0, Qt::CaseInsensitive);
    if (markerPos < 1) {
        m_modelSeries.clear();
        return false;
    }

    m_modelSeries = value.left(markerPos + 2);
    const int capacityStart = markerPos + 3;
    const int slashPos = value.indexOf(QLatin1Char('/'), capacityStart);
    if (slashPos < 0) {
        return false;
    }

    bool ok = false;
    QString token = value.mid(capacityStart, slashPos - capacityStart).trimmed();
    const double capacity = token.toDouble(&ok);
    if (!ok || capacity <= 0.0) {
        return false;
    }
    m_modelCapacity_kVA = capacity;

    const int voltageSep = value.indexOf(QLatin1Char('-'), slashPos + 1);
    if (voltageSep < 0) {
        return true;
    }
    token = value.mid(slashPos + 1, voltageSep - slashPos - 1).trimmed();
    const double hv = token.toDouble(&ok);
    if (ok && hv > 0.0) {
        m_modelHvRated_kV = hv;
    }

    if (!commitLowVoltage) {
        return true;
    }
    token = value.mid(voltageSep + 1).trimmed();
    const double lv = token.toDouble(&ok);
    if (ok && lv > 0.0) {
        m_modelLvRated_kV = lv;
    }
    return true;
}

QString ParamTableWidget::standardKey() const
{
    const auto ref = m_inputRefs.value(QStringLiteral("connectionGroup"), {-1, -1});
    const auto freq = m_inputRefs.value(QStringLiteral("frequency"), {-1, -1});
    return QStringLiteral("%1/%2/%3/%4/%5/%6/%7/%8/%9")
        .arg(m_modelSeries.toUpper()).arg(m_modelCapacity_kVA, 0, 'g', 12)
        .arg(m_modelHvRated_kV, 0, 'g', 12).arg(m_modelLvRated_kV, 0, 'g', 12)
        .arg(item(ref.first, ref.second) ? int(connectionType(item(ref.first, ref.second)->text())) : -1)
        .arg(item(freq.first, freq.second) ? item(freq.first, freq.second)->text() : QString())
        .arg(int(m_config.coreType)).arg(int(m_config.category)).arg(int(m_config.windingForm));
}

QString ParamTableWidget::standardUnavailableReason() const
{
    if (m_config.category != StructureConfig::OilImmersed || m_config.coreType != StructureConfig::StackedSilicon ||
        m_config.windingForm != StructureConfig::Dual)
        return QStringLiteral("当前结构不适用内置叠铁芯双绕组损耗标准表");
    if (std::abs(m_modelHvRated_kV - 10.0) > 1e-9)
        return QStringLiteral("内置损耗标准表仅收录10kV高压等级");
    const auto frequency = m_inputRefs.value(QStringLiteral("frequency"), {-1, -1});
    if (!item(frequency.first, frequency.second) || item(frequency.first, frequency.second)->text().toInt() != 50)
        return QStringLiteral("内置损耗标准联动仅用于50Hz配置");
    if (ModelStdTable::table(m_modelSeries).isEmpty())
        return QStringLiteral("型号系列“%1”未收录，不能套用SB20标准").arg(m_modelSeries);
    if (!ModelStdTable::lookup(m_modelSeries, m_modelCapacity_kVA))
        return QStringLiteral("容量%1kVA未收录，不能沿用其他容量的标准值").arg(m_modelCapacity_kVA);
    const auto connection = m_inputRefs.value(QStringLiteral("connectionGroup"), {-1, -1});
    if (!item(connection.first, connection.second)
        || !hasBuiltInConnectionStandards(connectionType(item(connection.first, connection.second)->text())))
        return QStringLiteral("当前联结组别无对应的内置损耗标准");
    return QString();
}

void ParamTableWidget::refreshStandardSelectorTip()
{
    if (m_loading) return;
    static const QRegularExpression modelPattern(
        QStringLiteral("^([A-Za-z0-9]+-M)-([0-9]+(?:\\.[0-9]+)?)/([0-9]+(?:\\.[0-9]+)?)-([0-9]+(?:\\.[0-9]+)?)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto model = modelPattern.match(m_productModelEdit ? m_productModelEdit->text().trimmed() : QString());
    const bool modelReady = model.hasMatch()
        && model.captured(1).compare(m_modelSeries, Qt::CaseInsensitive) == 0
        && model.captured(2).toDouble() == m_modelCapacity_kVA
        && model.captured(3).toDouble() == m_modelHvRated_kV
        && model.captured(4).toDouble() == m_modelLvRated_kV;
    const QString modeName = m_standardMode == TransformerParams::StandardMode::BuiltIn
        ? QStringLiteral("国标") : m_standardMode == TransformerParams::StandardMode::Custom
        ? QStringLiteral("非标") : QStringLiteral("待确认");
    const QString status = modelReady ? m_standardStatus
        : QStringLiteral("产品型号未完整提交；当前模式为%1。旧匹配状态无效，请补全型号并按回车提交低压电压后核对；当前保留的指标不代表已匹配此规格。").arg(modeName);
    const QString tip = QStringLiteral("指标来源模式：国标使用内置表自动匹配损耗及原单空载电流口径，不代表完整国标认证；非标使用手工指标。\n") + status;
    if (m_standardModeRow >= 0) item(m_standardModeRow, 2)->setToolTip(tip);
    if (m_standardModeCombo) m_standardModeCombo->setToolTip(tip);
}

void ParamTableWidget::applyModelLinkage()
{
    updateTestVoltageHints();
    if (m_loading) return;
    if (m_modelSeries.isEmpty() || m_modelCapacity_kVA <= 0.0) {
        refreshStandardSelectorTip();
        return;
    }
    const QString series = m_modelSeries;
    const double cap = m_modelCapacity_kVA;
    const auto cellText = [this](const QString &key) -> QString {
        const auto it = m_inputRefs.constFind(key);
        if (it == m_inputRefs.constEnd() || !item(it->first, it->second)) {
            return QString();
        }
        return item(it->first, it->second)->text().trimmed();
    };
    const auto setCell = [this](const QString &key, const QString &text) {
        const auto it = m_inputRefs.constFind(key);
        if (it != m_inputRefs.constEnd() && item(it->first, it->second)) {
            item(it->first, it->second)->setText(text);
        }
    };
    const QString key = standardKey();
    const QString unavailable = standardUnavailableReason();
    const auto currentRef = m_inputRefs.value(QStringLiteral("noLoadCurrentStd"), {-1, -1});
    if (auto *current = item(currentRef.first, currentRef.second)) {
        const QSignalBlocker blocker(this);
        current->setFlags(!m_baseParams.hasManualNoLoadCurrent()
            ? current->flags() & ~Qt::ItemIsEditable : current->flags() | Qt::ItemIsEditable);
    }
    if (m_standardMode == TransformerParams::StandardMode::Unconfirmed) {
        m_standardStatus = QStringLiteral("指标来源待确认：旧方案或指标数据缺失；保留当前值，请选择模式并逐项核对，不能视为该方案原指标。");
    } else if (m_standardMode == TransformerParams::StandardMode::Custom) {
        m_standardStatus = QStringLiteral("非标：使用手工性能指标；更改型号、容量和接法不自动覆盖指标，不代表已满足合同要求。");
    } else if (!unavailable.isEmpty()) {
        m_standardStatus = unavailable + QStringLiteral("；需手工核对：保留当前指标（含空载电流），未自动匹配当前规格。");
    } else {
        if (!m_lastLinkageKey.isEmpty() && key != m_lastLinkageKey)
            m_lossStandardsManual = false;
        if (!m_lossStandardsManual) {
            const auto *e = ModelStdTable::lookup(series, cap);
            const QSignalBlocker blocker(this);
            const bool yyn0 = connectionType(cellText(QStringLiteral("connectionGroup"))) == SupportedConnection::Yyn0;
            const double loadW = yyn0 ? e->loadYyn0_W : e->loadDyn_W;
            setCell(QStringLiteral("noLoadLossStd"), QString::number(e->noLoad_W));
            setCell(QStringLiteral("loadLossStd"), QString::number(loadW));
            setCell(QStringLiteral("totalLossStd"), QString::number(e->noLoad_W + loadW));

        }
        m_lastLinkageKey = key;
        m_standardStatus = QStringLiteral("%1/%2kVA：%3；阻抗保留手工值。")
            .arg(series).arg(cap)
            .arg(m_lossStandardsManual ? QStringLiteral("使用手工损耗标准") : QStringLiteral("已匹配内置10kV损耗表"));
    }
    // 空载电流来源独立于标准模式与损耗覆盖标志。
    if(!m_baseParams.hasManualNoLoadCurrent()){
        const QSignalBlocker blocker(this);double current=0;
        if(m_standardMode==TransformerParams::StandardMode::BuiltIn&&unavailable.isEmpty()&&ModelStdTable::originalNoLoadCurrent(cap,current))setCell(QStringLiteral("noLoadCurrentStd"),PerformanceCriteria::valueText(current));
        else if(m_standardMode==TransformerParams::StandardMode::Custom&&m_baseParams.hasNonstandardAn3)setCell(QStringLiteral("noLoadCurrentStd"),PerformanceCriteria::valueText(QString::number(m_baseParams.nonstandardAn3_pct-0.34,'g',12).toDouble()));
        else if(m_standardMode==TransformerParams::StandardMode::Custom)setCell(QStringLiteral("noLoadCurrentStd"),QString());
    }
    const QString currentStatus=m_baseParams.hasManualNoLoadCurrent()?QStringLiteral("空载电流手工固定，切换标准不覆盖"):
        m_standardMode==TransformerParams::StandardMode::Custom?(m_baseParams.hasNonstandardAn3?QStringLiteral("空载电流按原单AN3减0.34自动换算"):QStringLiteral("空载电流自动来源待填AN3，不能确认")):QStringLiteral("空载电流按原单查表减0.34自动取值");
    m_standardStatus+=QStringLiteral("；")+currentStatus+QStringLiteral("；原单规则不代表完整官方标准核验。");
    if(m_currentSourceRow>=0){item(m_currentSourceRow,5)->setText(currentStatus);item(m_currentSourceRow,5)->setToolTip(m_standardStatus);}
    for (const QString &lossKey : {QStringLiteral("noLoadLossStd"), QStringLiteral("loadLossStd"), QStringLiteral("totalLossStd")}) {
        const auto ref = m_inputRefs.value(lossKey);
        const QSignalBlocker blocker(this);
        item(ref.first, 5)->setText(m_standardMode == TransformerParams::StandardMode::Unconfirmed
            ? QStringLiteral("来源待确认（保留当前值）") : m_standardMode == TransformerParams::StandardMode::Custom
            ? QStringLiteral("非标手工指标") : unavailable.isEmpty()
            ? (m_lossStandardsManual ? QStringLiteral("手工设置") : QStringLiteral("内置10kV表"))
            : QStringLiteral("需手工核对（保留原值）"));
        item(ref.first, ref.second)->setToolTip(m_standardStatus);
    }
    if (m_standardModeRow >= 0) {
        const QSignalBlocker blocker(this);
        item(m_standardModeRow, 5)->setText(m_standardMode == TransformerParams::StandardMode::BuiltIn
            ? (unavailable.isEmpty() ? (m_lossStandardsManual?QStringLiteral("损耗手工覆盖；空载电流按独立来源"):QStringLiteral("损耗查表；空载电流按独立来源")) : QStringLiteral("指标未匹配，需手工核对")) : m_standardMode == TransformerParams::StandardMode::Custom
            ? QStringLiteral("全部指标手工；规格切换不覆盖") : QStringLiteral("旧方案来源缺失，请核对"));
        item(m_standardModeRow, 5)->setToolTip(m_standardStatus);
    }
    refreshStandardSelectorTip();
    emit stdValuesUpdated(m_standardStatus);
    updateYokePiece1();
    updateLvTurnsRecommendation();
    updateDesignConnection();
    scheduleDesignCore();
}

void ParamTableWidget::scheduleDesignCore()
{
    if (m_loading || !m_designConnectionUi || m_designCoreRow < 0) return;
    updateLaminationPreview(); // Independent, cheap geometry; never waits for a valid full design.
    showDesignCore(nullptr, QStringLiteral("正在更新当前输入；旧预览已失效"));
    m_corePreviewTimer->start();
}

void ParamTableWidget::addLaminationPreview(int &row)
{
    const int headerRow = row++;
    addParamRow(headerRow, QString(), QString());
    const int bodyRow = row++;
    addParamRow(bodyRow, QString(), QString());
    for (int r : {headerRow, bodyRow}) {
        for (int col = 0; col < columnCount(); ++col)
            item(r, col)->setFlags(item(r, col)->flags() & ~Qt::ItemIsEditable);
        setSpan(r, 1, 1, 5);
    }
    auto *toggle = new QToolButton(this);
    toggle->setText(QStringLiteral("圆形/长圆形几何叠积预览（只读，不参与整机计算）"));
    toggle->setCheckable(true);
    toggle->setChecked(m_laminationExpanded);
    toggle->setArrowType(m_laminationExpanded ? Qt::DownArrow : Qt::RightArrow);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toggle->setStyleSheet(QStringLiteral("QToolButton { text-align:left; color:#24362B; background:#DCEEDF; border:0; padding:4px; font-weight:bold; }"));
    toggle->setToolTip(QStringLiteral("跟随上方心柱选型，默认收起；只显示原单几何片宽及半叠厚，不开放完整计算、保存或寻优。"));
    setCellWidget(headerRow, 1, toggle);
    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);
    m_laminationStatus = new QLabel(body);
    m_laminationStatus->setTextFormat(Qt::PlainText);
    m_laminationStatus->setWordWrap(true);
    m_laminationStatus->setFixedHeight(72);
    layout->addWidget(m_laminationStatus);
    m_laminationTable = new QTableWidget(15, 5, body);
    m_laminationTable->setHorizontalHeaderLabels({QStringLiteral("原单位置"), QStringLiteral("片宽(mm)"),
        QStringLiteral("几何半叠厚(mm)"), QStringLiteral("圆弧累计(mm)"), QStringLiteral("用途说明")});
    m_laminationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_laminationTable->setAlternatingRowColors(true);
    m_laminationTable->verticalHeader()->hide();
    m_laminationTable->verticalHeader()->setDefaultSectionSize(24);
    m_laminationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_laminationTable->setStyleSheet(QStringLiteral("QTableWidget { background:#FFFFFF; alternate-background-color:#F2F7F3; gridline-color:#D3DDD6; color:#24362B; } QHeaderView::section { background:#DCEEDF; color:#24362B; padding:3px; border:1px solid #D3DDD6; }"));
    const QStringList mainColumns = {"G", "H", "I", "J", "K", "L", "M", "N", "O", "Q", "S"};
    const QStringList supplements = {"F", "G", "H", "I"};
    for (int i = 0; i < 15; ++i) {
        const QString location = i < 11 ? mainColumns[i] + QStringLiteral("17/18")
                                       : supplements[i - 11] + QStringLiteral("19/20");
        for (int col = 0; col < 5; ++col) {
            auto *cell = new QTableWidgetItem(col == 0 ? location : col == 4
                ? (i < 11 ? QStringLiteral("主叠积几何") : QStringLiteral("补充片几何，非手工实际值")) : QStringLiteral("—"));
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            cell->setTextAlignment(col >= 1 && col <= 3 ? Qt::AlignRight | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter);
            cell->setToolTip(QStringLiteral("几何半叠厚逐级取整；累计仅含圆弧部分，不含长圆中心矩形，不是Sheet1片数取整后的实际叠厚。"));
            m_laminationTable->setItem(i, col, cell);
        }
    }
    layout->addWidget(m_laminationTable, 1);
    setCellWidget(bodyRow, 1, body);
    setRowHeight(bodyRow, 374);
    setRowHidden(bodyRow, !m_laminationExpanded);
    connect(toggle, &QToolButton::toggled, this, [this, toggle, bodyRow](bool expanded) {
        m_laminationExpanded = expanded;
        toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        setRowHidden(bodyRow, !expanded);
    });
}

void ParamTableWidget::updateLaminationPreview()
{
    if (m_loading || !m_designConnectionUi || !m_laminationTable || !m_laminationStatus) return;
    QString error;
    double diameter = 0.0, straight = 0.0;
    const auto readNumber = [this, &error](const QString &key, double &value) {
        const auto ref = m_inputRefs.value(key, {-1, -1});
        const auto *cell = item(ref.first, ref.second);
        bool ok = false;
        value = cell ? cell->text().toDouble(&ok) : 0.0;
        if (!ok || !std::isfinite(value)) error = QStringLiteral("几何输入未完整填写，请核对直径H12和长圆形直线段K12；不沿用旧值。");
    };
    if (m_config.coreType != StructureConfig::StackedSilicon)
        error = QStringLiteral("本区仅用于叠铁芯几何预览；其他铁芯结构未接入。" );
    else if (m_config.coreShape != StructureConfig::Circle && m_config.coreShape != StructureConfig::LongRound)
        error = QStringLiteral("本区仅预览圆形/长圆形；椭圆沿用上方已有结果，半椭圆/类椭圆未接入。" );
    else {
        readNumber(QStringLiteral("coreDiameter"), diameter);
        if (m_config.coreShape == StructureConfig::LongRound) readNumber(QStringLiteral("coreStraight"), straight);
    }
    auto &db = DesignDatabase::instance();
    CoreLaminationPreview::Result result;
    if (error.isEmpty() && !db.isLoaded() && !db.load())
        error = QStringLiteral("项目叠积数据加载失败：") + db.lastError();
    if (error.isEmpty()) {
        QVector<double> offsets;
        for(int i=0;i<20;++i){
            double value=m_recommendationBaseInput.coreWidthOffsets_mm[i];
            const auto key=QStringLiteral("coreWidthOffset_%1").arg(i);
            if(m_inputRefs.contains(key))readNumber(key,value);
            offsets.append(value);
        }
        if(error.isEmpty())result = CoreLaminationPreview::calculate(m_config.coreShape, diameter, straight, db.coreRows(),offsets);
        if(error.isEmpty())error = result.error;
    }
    const bool available = error.isEmpty() && result.available;
    const QSignalBlocker blocker(m_laminationTable);
    for (int i = 0; i < 15; ++i)
        for (int col = 1; col <= 3; ++col) {
            const double value = !available ? 0.0 : col == 1 ? result.stages[i].width_mm
                : col == 2 ? result.stages[i].stack_mm : result.stages[i].arcCumulative_mm;
            m_laminationTable->item(i, col)->setText(available ? QString::number(value, 'g', 15) : QStringLiteral("—"));
        }
    QString status = error;
    if (available) {
        const QString rectangle = m_config.coreShape == StructureConfig::LongRound
            ? QStringLiteral("中心矩形：宽%1mm、全厚%2mm（B20/D20），与圆弧部分分开显示。")
                .arg(result.centralWidth_mm).arg(result.centralFullThickness_mm)
            : QStringLiteral("圆形本预览不使用K12；原直线段输入保留，不静默改为0。");
        status = QStringLiteral("%1几何预览：H12=%2mm，项目查表档%3mm（阶梯取档，外链未重算）。\n%4\n仅几何半叠厚；非实际截面、磁密或重量，整机计算/保存/寻优仍未开放。")
            .arg(m_config.coreShape == StructureConfig::Circle ? QStringLiteral("圆形") : QStringLiteral("长圆形"))
            .arg(diameter).arg(result.matchedReference_mm).arg(rectangle);
    }
    m_laminationStatus->setText(status);
    m_laminationStatus->setToolTip(status);
}

void ParamTableWidget::showDesignCore(const CalcResult *result, const QString &reason)
{
    if (!m_designConnectionUi || m_designCoreRow < 0) return;
    const QSignalBlocker blocker(this);
    const double values[3][2] = {
        {result ? result->core.coreAreaActual_cm2 : 0.0, result ? result->core.yokeAreaActual_cm2 : 0.0},
        {result ? result->core.fluxDensity_coreActual_T : 0.0, result ? result->core.fluxDensity_yoke_T : 0.0},
        {result ? result->core.coreWeight_kg : 0.0, result ? result->core.noLoadLoss_W : 0.0}
    };
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 2; ++c) {
            auto *cell = item(m_designCoreRow + r, c == 0 ? 2 : 4);
            cell->setText(result ? QString::number(values[r][c], 'f', r == 0 ? 2 : r == 1 ? 3 : 0)
                                 : QStringLiteral("不可用"));
            cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            cell->setToolTip(result
                ? QStringLiteral("本次引擎结果；截面与磁密采用片数取整后的实际值；只读预览，不代表指标合格。") : reason);
        }
        auto *remark = item(m_designCoreRow + r, 5);
        remark->setText(result ? QStringLiteral("当前输入引擎预览；非合格判定") : reason);
        remark->setToolTip(remark->text());
    }
}

void ParamTableWidget::updateDesignCore()
{
    if (m_loading || !m_designConnectionUi || m_designCoreRow < 0) return;
    QString error;
    const QString modelText = m_productModelEdit->text().trimmed();
    static const QRegularExpression pattern(
        QStringLiteral("^([A-Za-z0-9]+-M)-([0-9]+(?:\\.[0-9]+)?)/([0-9]+(?:\\.[0-9]+)?)-([0-9]+(?:\\.[0-9]+)?)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto model = pattern.match(modelText);
    if (!PerformanceCriteria::validModel(modelText) || !model.hasMatch()
        || model.captured(2).toDouble() != m_modelCapacity_kVA
        || model.captured(3).toDouble() != m_modelHvRated_kV
        || model.captured(4).toDouble() != m_modelLvRated_kV)
        error = QStringLiteral("请补全产品型号；低压额定电压修改后按回车提交");
    if (error.isEmpty() && (!m_tapPlusSpin->hasAcceptableInput() || !m_tapMinusSpin->hasAcceptableInput()
        || !m_tapStepSpin->hasAcceptableInput())) error = QStringLiteral("调压输入尚未完成");
    // 隐藏表不能用宽=厚推断圆线，绕过主页面要求的显式线形选择。
    if (error.isEmpty() && m_wireFormCombo && !m_wireFormCombo->currentData().toBool()) {
        const auto width = m_inputRefs.value(QStringLiteral("hvBareWidth"));
        const auto thick = m_inputRefs.value(QStringLiteral("hvBareThick"));
        bool widthOk = false, thickOk = false;
        const double w = item(width.first, width.second)->text().toDouble(&widthOk);
        const double t = item(thick.first, thick.second)->text().toDouble(&thickOk);
        if (widthOk && thickOk && w == t)
            error = QStringLiteral("宽=厚按计算单属于圆线，请明确选择圆线并选取表内规格。");
    }
    CalcInput preview = m_recommendationBaseInput;
    TransformerParams params = getParams();
    CalcResult result;
    if (error.isEmpty()) {
        saveToInput(preview);
        // 验证使用独立表格，不能让预览提交型号、覆盖原指标或改变手工匝数。
        ParamTableWidget validation;
        validation.loadParamsForConfig(params, m_config, preview,
            m_config.calcMode == StructureConfig::Professional);
        {
            const QSignalBlocker blocker(&validation);
            validation.m_productModelEdit->setText(modelText);
            // 保留当前屏幕文本的精度和非法输入，不允许重新加载时回退旧值。
            for (auto it = m_inputRefs.constBegin(); it != m_inputRefs.constEnd(); ++it) {
                const auto target = validation.m_inputRefs.constFind(it.key());
                if (target != validation.m_inputRefs.constEnd())
                    validation.item(target->first, target->second)->setText(item(it->first, it->second)->text());
            }
        }
        if (validation.collectForCalculation(params, preview, error, false)) {
            CoreResult geometry;
            if (ElectromagneticEngine::previewCoreGeometry(preview, geometry, error)) {
                ElectromagneticEngine engine;
                if (!engine.calcElectromagnetic(preview, result) || !result.valid)
                    error = result.error.isEmpty() ? QStringLiteral("当前输入计算未成功") : result.error;
            }
        }
    }
    showDesignCore(error.isEmpty() && result.valid ? &result : nullptr,
        error.isEmpty() ? QStringLiteral("当前输入尚不能生成铁芯预览") : error);
}

void ParamTableWidget::updateDesignConnection()
{
    if (m_loading || !m_designConnectionUi || m_designPhaseRow < 0) return;
    const QSignalBlocker blocker(this);
    QString error;
    const auto type = connectionType(item(m_designConnectionRow, 2)->text());
    const bool needsCustom = type == SupportedConnection::Dd || type == SupportedConnection::Yd;
    const bool previewOnly = needsCustom && !hasSupportedConnectionGroup();
    const QString note = hasBuiltInConnectionStandards(type)
        ? (type == SupportedConnection::Dyn11 ? QStringLiteral("高压D / 低压yn；内置匹配时按Dyn列")
                                             : QStringLiteral("高压Y / 低压yn；内置匹配时按Yyn列"))
        : needsCustom
            ? (type == SupportedConnection::Dd ? QStringLiteral("高压D / 低压d") : QStringLiteral("高压Y / 低压d"))
                + (previewOnly ? QStringLiteral("；当前仅相电量预览，须选非标并核对手工指标后计算/保存")
                               : QStringLiteral("；非标计算，使用手工指标，无对应内置标准列"))
            : QStringLiteral("当前接法无法识别；禁止计算，不沿用旧接法");
    item(m_designConnectionRow, 5)->setText(note + QStringLiteral("；仅50Hz"));
    item(m_designConnectionRow, 5)->setToolTip(item(m_designConnectionRow, 5)->text());
    if (type == SupportedConnection::Invalid)
        error = QStringLiteral("当前联结组别无法识别，不能预览相电量");
    static const QRegularExpression modelPattern(
        QStringLiteral("^([A-Za-z0-9]+-M)-([0-9]+(?:\\.[0-9]+)?)/([0-9]+(?:\\.[0-9]+)?)-([0-9]+(?:\\.[0-9]+)?)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto model = modelPattern.match(m_productModelEdit->text().trimmed());
    if (error.isEmpty() && (!model.hasMatch()
        || model.captured(2).toDouble() != m_modelCapacity_kVA
        || model.captured(3).toDouble() != m_modelHvRated_kV
        || model.captured(4).toDouble() != m_modelLvRated_kV))
        error = QStringLiteral("请补全产品型号；低压电压修改后按回车提交");
    const auto frequency = m_inputRefs.value(QStringLiteral("frequency"));
    if (error.isEmpty() && item(frequency.first, frequency.second)->text().toInt() != 50)
        error = QStringLiteral("当前联动仅支持50Hz");
    const auto turns = m_inputRefs.value(QStringLiteral("lvTurns"));
    bool turnsOk = false;
    const int lvTurns = item(turns.first, turns.second)->text().toInt(&turnsOk);
    if (error.isEmpty() && (!turnsOk || lvTurns <= 0)) error = QStringLiteral("请填写有效的实际低压匝数");
    if (error.isEmpty() && (!m_tapPlusSpin->hasAcceptableInput() || !m_tapMinusSpin->hasAcceptableInput()
        || !m_tapStepSpin->hasAcceptableInput())) error = QStringLiteral("调压数值尚未输入完整");
    ElectricalResult electrical;
    if (error.isEmpty()) {
        CalcInput preview = m_recommendationBaseInput;
        saveToInput(preview);
        // Basic preview is available even while full calculation is mode-blocked.
        preview.capacity_kVA = m_modelCapacity_kVA;
        preview.hvRated_kV = m_modelHvRated_kV;
        preview.lvRated_kV = m_modelLvRated_kV;
        ElectromagneticEngine::previewElectrical(preview, electrical, error);
    }
    const double values[2][2] = {{electrical.hvPhaseRated_V, electrical.lvPhase_V},
                                 {electrical.hvPhaseCurrent_A, electrical.lvPhaseCurrent_A}};
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 2; ++c) {
            auto *cell = item(m_designPhaseRow + r, c == 0 ? 2 : 4);
            cell->setText(error.isEmpty() ? QString::number(values[r][c], 'f', r == 0 ? 0 : 2) : QStringLiteral("不可用"));
            cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            cell->setToolTip(error.isEmpty()
                ? (previewOnly ? QStringLiteral("Dd/Yd当前仅相电量预览；须选非标并核对手工指标后计算及保存，不改实际匝数或套用内置标准。")
                               : QStringLiteral("当前已提交额定值与接法，复用引擎基础电量公式；只读，不改实际匝数。"))
                : error);
        }
        auto *remark = item(m_designPhaseRow + r, 5);
        remark->setText(error.isEmpty()
            ? (previewOnly ? QStringLiteral("仅相电量预览；完整计算及保存须选非标手工指标")
                           : QStringLiteral("仅基础电量联动；不代表完整方案可算或合格"))
            : error);
        remark->setToolTip(remark->text());
    }
}

void ParamTableWidget::updateTestVoltageHints()
{
    if (m_loading || m_testVoltageRow < 0) return;
    const QSignalBlocker blocker(this);
    const auto hints = testVoltageHints(m_modelHvRated_kV, m_modelLvRated_kV);
    item(m_testVoltageRow, 2)->setText(hints.highVoltage);
    item(m_testVoltageRow, 4)->setText(hints.lowVoltage);
    item(m_testVoltageRow, 2)->setToolTip(hints.highReason);
    item(m_testVoltageRow, 4)->setToolTip(hints.lowReason);
    item(m_testVoltageRow, 5)->setText(QStringLiteral("计算单参考；非合格判定"));
    item(m_testVoltageRow, 5)->setToolTip(TestVoltageHints::sourceNote()
        + QStringLiteral("；") + hints.highReason + QStringLiteral("；") + hints.lowReason);
}

void ParamTableWidget::updateYokePiece1()
{
    if (m_loading || !m_yokePiece1ModeCombo) return;
    const auto ref = m_inputRefs.value(QStringLiteral("yokePiece1Stack"));
    auto *valueItem = item(ref.first, ref.second);
    if (!valueItem) return;
    if (valueItem->flags() & Qt::ItemIsEditable) {
        bool ok = false;
        const double value = valueItem->text().toDouble(&ok);
        if (ok && std::isfinite(value) && value >= 0.0) m_manualYokePiece1_mm = value;
    }
    const bool automatic = m_yokePiece1ModeCombo->currentData().toBool();
    const QSignalBlocker blocker(this);
    valueItem->setFlags(automatic ? valueItem->flags() & ~Qt::ItemIsEditable
                                 : valueItem->flags() | Qt::ItemIsEditable);
    if (!automatic) {
        // 手工模式编辑期间不要用缓存覆盖屏幕上的非法输入，留给收集校验拒绝。
        if (valueItem->data(Qt::UserRole).toBool())
            valueItem->setText(QString::number(m_manualYokePiece1_mm, 'g', 15));
        valueItem->setData(Qt::UserRole, false);
        valueItem->setToolTip(QStringLiteral("手工叠厚参与计算；切换自动后仍独立保存本值"));
        item(ref.first, 5)->setText(QStringLiteral("手工值参与计算；宽80、宽60仍手填"));
        return;
    }
    valueItem->setData(Qt::UserRole, true);
    CalcInput preview = m_recommendationBaseInput;
    saveToInput(preview);
    QString error;
    for (const QString &key : {QStringLiteral("coreDiameter"), QStringLiteral("coreStraight"),
                              QStringLiteral("ellipseAngle"), QStringLiteral("stackFactor"),
                              QStringLiteral("yokePiece2Stack"), QStringLiteral("yokePiece3Stack")}) {
        const auto it = m_inputRefs.constFind(key);
        if (it == m_inputRefs.constEnd()) continue;
        bool ok = false;
        const double value = item(it->first, it->second)->text().toDouble(&ok);
        if (!ok || !std::isfinite(value)) { error = QStringLiteral("铁芯几何输入未有效填写"); break; }
    }
    if (m_config.coreShape != StructureConfig::Ellipse || m_config.coreType != StructureConfig::StackedSilicon)
        error = QStringLiteral("自动叠厚仅用于已支持的椭圆叠铁芯");
    CoreResult core;
    if (error.isEmpty()) ElectromagneticEngine::previewCoreGeometry(preview, core, error);
    valueItem->setText(error.isEmpty() ? QString::number(core.yokePiece1Stack_mm, 'g', 15) : QStringLiteral("不可用"));
    valueItem->setToolTip(error.isEmpty()
        ? QStringLiteral("按F20自动计算，实际片宽%1mm；手工备份%2mm。自动值参与截面及损耗计算。")
              .arg(core.yokePiece1Width_mm).arg(m_manualYokePiece1_mm) : error);
    item(ref.first, 5)->setText(error.isEmpty() ? QStringLiteral("F20自动值参与计算；原手工值独立保留") : error);
}

void ParamTableWidget::updateLvTurnsRecommendation()
{
    if (m_loading || m_recommendationRow < 0) return;
    const QSignalBlocker blocker(this);
    auto *recommended = item(m_recommendationRow, 4);
    QString error;
    CalcInput preview = m_recommendationBaseInput;
    saveToInput(preview);
    preview.lvRated_kV = m_modelLvRated_kV;
    // 不让正在编辑的非法值回退为原值后给出看似有效的推荐。
    for (const QString &key : {QStringLiteral("refFluxDens"), QStringLiteral("coreDiameter"),
                              QStringLiteral("coreStraight"), QStringLiteral("ellipseAngle"),
                              QStringLiteral("stackFactor"), QStringLiteral("yokePiece1Stack"),
                              QStringLiteral("yokePiece2Stack"), QStringLiteral("yokePiece3Stack")}) {
        const auto it = m_inputRefs.constFind(key);
        if (it == m_inputRefs.constEnd()) continue;
        bool ok = false;
        const double value = item(it->first, it->second)->text().toDouble(&ok);
        if (!ok || !std::isfinite(value)) { error = QStringLiteral("推荐所需输入尚未有效填写"); break; }
    }
    const auto freqRef = m_inputRefs.value(QStringLiteral("frequency"));
    if (m_config.coreShape != StructureConfig::Ellipse || m_config.coreType != StructureConfig::StackedSilicon
            || item(freqRef.first, freqRef.second)->text().toInt() != 50 || !hasSupportedConnectionGroup())
        error = QStringLiteral("当前推荐仅适用于已支持的椭圆叠铁芯、50Hz和联结组别");
    const auto r = error.isEmpty() ? ElectromagneticEngine::recommendLvTurns(preview) : LvTurnsRecommendation();
    if (error.isEmpty()) error = r.error;
    recommended->setText(error.isEmpty() ? QString::number(r.turns) : QStringLiteral("不可用"));
    recommended->setToolTip(error.isEmpty()
        ? QStringLiteral("AN8：相电压%1V ÷ 参考匝电压%2V（先取4位），取整数；心柱截面%3cm²。仅建议，不改实际匝数。")
              .arg(r.phaseVoltage_V).arg(r.referenceTurnVoltage_V, 0, 'f', 4).arg(r.coreArea_cm2, 0, 'f', 2)
        : error);
    item(m_recommendationRow, 5)->setText(error.isEmpty() ? QStringLiteral("仅建议；实际计算使用手填低压匝数") : error);
}

void ParamTableWidget::updateWireInsulation()
{
    if (!m_wireInsulationCombo || !m_inputRefs.contains(QStringLiteral("hvWireInsulAdd")))
        return;
    const auto ref = m_inputRefs.value(QStringLiteral("hvWireInsulAdd"));
    auto *valueItem = item(ref.first, ref.second);
    if (!valueItem) return;
    if (m_wireFormCombo && m_wireFormCombo->currentData().toBool()) {
        WireSpec spec;
        const bool valid = m_roundWireSpecCombo && m_roundWireSpecCombo->currentData().isValid()
            && DesignDatabase::instance().roundWireSpec(m_roundWireSpecCombo->currentData().toDouble(), spec);
        const QSignalBlocker blocker(this);
        valueItem->setText(QString::number(valid ? spec.insulatedWidthMm - spec.bareWidthMm : 0.0, 'g', 15));
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setToolTip(QStringLiteral("圆线总增厚=表内绝缘后直径-裸线直径；不使用扁线预设或手填值"));
        item(ref.first, 5)->setText(valid
            ? QStringLiteral("按计算单圆线表；绝缘后直径%1mm，增重%2%；不代表所有材料/绝缘等级").arg(spec.insulatedWidthMm).arg(spec.weightAddPct)
            : QStringLiteral("圆线规格或表数据无效，禁止计算"));
        return;
    }
    // 仅在离开自定义时记住手填值，切换预设不会覆盖这份会话值。
    if (!m_loading && (valueItem->flags() & Qt::ItemIsEditable)) {
        bool ok = false;
        const double value = valueItem->text().toDouble(&ok);
        if (ok && std::isfinite(value) && value >= 0.0) m_customWireInsulAdd_mm = value;
    }
    const double preset = CalcInput::insulationIncrement(m_wireInsulationCombo->currentData().toString());
    const bool custom = preset < 0.0;
    const QSignalBlocker blocker(this);
    valueItem->setText(QString::number(custom ? m_customWireInsulAdd_mm : preset, 'g', 15));
    valueItem->setFlags(custom ? valueItem->flags() | Qt::ItemIsEditable
                               : valueItem->flags() & ~Qt::ItemIsEditable);
    const QString note = custom ? QStringLiteral("手填总增厚；旧方案原值保留")
                               : QStringLiteral("按计算单自动增厚；如需手填请选择自定义");
    valueItem->setToolTip(note + QStringLiteral("；绝缘宽/厚=裸宽/厚+增厚，不乘2"));
    item(ref.first, 5)->setText(note);
}

void ParamTableWidget::updateWireForm()
{
    if (!m_wireFormCombo || !m_roundWireSpecCombo || !m_wireInsulationCombo) return;
    const bool round = m_wireFormCombo->currentData().toBool();
    const auto widthRef = m_inputRefs.value(QStringLiteral("hvBareWidth"));
    const auto thickRef = m_inputRefs.value(QStringLiteral("hvBareThick"));
    auto *width = item(widthRef.first, widthRef.second);
    auto *thick = item(thickRef.first, thickRef.second);
    const QSignalBlocker tableBlocker(this);
    const QSignalBlocker insulationBlocker(m_wireInsulationCombo);
    if (round && !m_roundWireActive) {
        m_flatWireWidth = width->text();
        m_flatWireThick = thick->text();
        m_flatWireInsulation = m_wireInsulationCombo->currentData().toString();
        // 切换圆线前保留自定义扁线增厚。
        const auto addRef = m_inputRefs.value(QStringLiteral("hvWireInsulAdd"));
        if (m_flatWireInsulation == QLatin1String("Custom")) {
            bool ok = false;
            const double add = item(addRef.first, addRef.second)->text().toDouble(&ok);
            if (ok && std::isfinite(add) && add >= 0.0) m_customWireInsulAdd_mm = add;
        }
    }
    if (round) {
        if (m_roundWireSpecCombo->currentData().isValid()) {
            const QString diameter = QString::number(m_roundWireSpecCombo->currentData().toDouble(), 'g', 15);
            width->setText(diameter);
            thick->setText(diameter);
        }
        m_wireInsulationCombo->setCurrentIndex(m_wireInsulationCombo->findData(QStringLiteral("RoundTable")));
    } else {
        if (m_roundWireActive) {
            width->setText(m_flatWireWidth);
            thick->setText(m_flatWireThick);
        }
        m_wireInsulationCombo->setCurrentIndex(m_wireInsulationCombo->findData(m_flatWireInsulation));
    }
    width->setFlags(round ? width->flags() & ~Qt::ItemIsEditable : width->flags() | Qt::ItemIsEditable);
    thick->setFlags(round ? thick->flags() & ~Qt::ItemIsEditable : thick->flags() | Qt::ItemIsEditable);
    m_roundWireSpecCombo->setEnabled(round);
    m_wireInsulationCombo->setEnabled(!round);
    item(widthRef.first, 5)->setText(round ? QStringLiteral("宽、厚均为裸线直径；仅下拉框表内规格可计算")
                                         : QStringLiteral("扁线宽厚手填；宽=厚时请改选圆线表规格"));
    m_roundWireActive = round;
    updateWireInsulation();
}

// 根据当前结构配置动态生成参数表：不同铁芯/绕组组合显示不同的参数行和分段；
// 四/五/六节为可编辑设计变量，与 CalcInput 双向同步（saveToInput 读回）；
// proMode=true 时追加七~十节高级参数（专业模式）
void ParamTableWidget::setCalculationConfig(const StructureConfig &config)
{
    const bool coreChanged = m_config.coreShape != config.coreShape || m_config.yokeShape != config.yokeShape;
    m_config = config;
    applyModelLinkage();
    updateHvCoilHints();
    if (coreChanged) {
        updateYokePiece1();
        updateLvTurnsRecommendation();
        scheduleDesignCore();
        updateLaminationPreview();
    }
}

void ParamTableWidget::setCoreSelection(StructureConfig::CoreShape core, StructureConfig::YokeShape yoke)
{
    auto config = m_config;
    config.coreShape = core;
    config.yokeShape = yoke;
    setCalculationConfig(config);
}

void ParamTableWidget::loadParamsForConfig(const TransformerParams &params, const StructureConfig &config,
                                           const CalcInput &input, bool proMode, bool designConnectionUi,
                                           bool sheetSelectionUi)
{
    // 普通/专业模式会在同一对象上重载。销毁旧单元格前保留扁线会话备份；
    // 当前圆线输入不包含这份备份，不得用圆线增厚或默认宽厚覆盖它。
    if (m_wireFormCombo && !m_roundWireActive && m_wireInsulationCombo) {
        const auto widthRef = m_inputRefs.value(QStringLiteral("hvBareWidth"));
        const auto thickRef = m_inputRefs.value(QStringLiteral("hvBareThick"));
        m_flatWireWidth = item(widthRef.first, widthRef.second)->text();
        m_flatWireThick = item(thickRef.first, thickRef.second)->text();
        m_flatWireInsulation = m_wireInsulationCombo->currentData().toString();
        if (m_flatWireInsulation == QLatin1String("Custom")) {
            const auto addRef = m_inputRefs.value(QStringLiteral("hvWireInsulAdd"));
            bool ok = false;
            const double add = item(addRef.first, addRef.second)->text().toDouble(&ok);
            if (ok && std::isfinite(add) && add >= 0.0) m_customWireInsulAdd_mm = add;
        }
    }
    m_loading = true;
    m_corePreviewTimer->stop();
    m_designCoreRow = -1;
    m_laminationStatus = nullptr;
    m_laminationTable = nullptr;
    m_designConnectionUi = designConnectionUi;
    m_designConnectionCombo = nullptr;
    m_designConnectionRow = -1;
    m_designPhaseRow = -1;
    m_recommendationBaseInput = input;
    m_yokePiece1ModeCombo = nullptr;
    m_manualYokePiece1_mm = input.yokePiece1Stack_mm;
    m_recommendationRow = -1;
    m_testVoltageRow = -1;
    m_baseParams = params;
    m_baseParams.noLoadCurrentSource=params.hasManualNoLoadCurrent()?TransformerParams::NoLoadCurrentSource::Manual:TransformerParams::NoLoadCurrentSource::Automatic;
    m_config = config;
    // The design page's ribbon is editable and authoritative there. A saved
    // candidate elsewhere must retain its own shape, not the current page's.
    if (!designConnectionUi) {
        m_config.coreShape = input.coreShape;
        m_config.yokeShape = input.yokeShape;
    }
    m_lossStandardsManual = params.lossStandardsManual;
    m_standardMode = params.standardMode;
    m_currentSourceCombo=nullptr;m_currentSourceRow=-1;
    m_standardModeCombo = nullptr;
    m_standardModeRow = -1;
    m_lastLinkageKey = params.lossStandardsKey;
    m_tapPlusSpin = nullptr;
    m_tapMinusSpin = nullptr;
    m_tapStepSpin = nullptr;
    m_steelGradeCombo = nullptr;
    m_steelCurveRangeRow = -1;
    m_hvMaterialCombo = nullptr;
    m_lvMaterialCombo = nullptr;
    m_wireInsulationCombo = nullptr;
    m_wireFormCombo = nullptr;
    m_roundWireSpecCombo = nullptr;
    m_roundWireActive = input.isRoundHighVoltageWire();
    m_unlistedRoundDiameter_mm = input.hvBareWidth_mm;
    if (!m_roundWireActive) {
        m_flatWireWidth = QString::number(input.hvBareWidth_mm, 'g', 15);
        m_flatWireThick = QString::number(input.hvBareThick_mm, 'g', 15);
        m_flatWireInsulation = input.resolvedInsulationType();
        if (m_flatWireInsulation == QLatin1String("Custom")) m_customWireInsulAdd_mm = input.hvWireInsulAdd_mm;
    }
    clearSpans();
    setRowCount(0);
    m_inputRefs.clear();
    int row = 0;

    // 根据变压器结构类型生成缺省产品型号前缀
    QString modelPrefix;
    switch (config.coreType) {
    case StructureConfig::StackedSilicon:   modelPrefix = "S"; break;
    case StructureConfig::StereoscopicRoll: modelPrefix = "SZ"; break;
    case StructureConfig::PlanarAmorphous:  modelPrefix = "SBH"; break;
    }

    // 一 输入信息（所有类型共有；静态行同样绑定 key，getParams 按 key 读取）
    addSectionRow(row++, QStringLiteral("一 输入信息"),
                  QStringLiteral("变压器效率计算方式"), params.efficiencyCalcMethod);
    const QString oldModel = params.productModel.section(QLatin1Char('/'), 0, 0);
    const QRegularExpression oldModelPattern(
        QStringLiteral("^(.+-M)(?:-[0-9]+(?:[.][0-9]+)?){0,2}$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch oldModelMatch = oldModelPattern.match(oldModel);
    QString series;
    if (oldModelMatch.hasMatch()) {
        series = oldModelMatch.captured(1);
    } else if (config.coreType == StructureConfig::StackedSilicon) {
        // 历史缺省值“S13配变”不是铭牌系列，保持原行为并回落到 SB20-M。
        series = QStringLiteral("SB20-M");
    } else {
        series = modelPrefix + QStringLiteral("15-M");
    }
    const QString compositeModel = QStringLiteral("%1-%2/%3-%4")
        .arg(series,
             QString::number(params.capacity_kVA, 'g', 12),
             QString::number(params.hvRatedVoltage_kV, 'g', 12),
             QString::number(params.lvRatedVoltage_kV, 'g', 12));

    addParamRow(row, QStringLiteral("产品型号"), compositeModel);
    item(row, 5)->setText(QStringLiteral("型号-M-容量/高压-低压"));
    m_productModelEdit = new QLineEdit(this);
    m_productModelEdit->setText(compositeModel);
    m_productModelEdit->setPlaceholderText(QStringLiteral("例如：SB20-M-630/10-0.4"));
    m_productModelEdit->setToolTip(QStringLiteral(
        "输入“/”后保存容量并联动标准值；输入高压后的“-”保存高压；输入低压后按回车保存；小数使用“.”，不支持逗号"));
    setCellWidget(row, 2, m_productModelEdit);
    setSpan(row, 2, 1, 3);
    m_modelSeries = series;
    m_modelCapacity_kVA = params.capacity_kVA;
    m_modelHvRated_kV = params.hvRatedVoltage_kV;
    m_modelLvRated_kV = params.lvRatedVoltage_kV;
    connect(m_productModelEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        const bool capacityComplete = parseCompositeModel(text, false);
        if (capacityComplete) {
            applyModelLinkage();
        }
        refreshStandardSelectorTip();
    });
    connect(m_productModelEdit, &QLineEdit::returnPressed, this, [this]() {
        parseCompositeModel(m_productModelEdit->text(), true);
        applyModelLinkage();
    });
    ++row;

    addInputRow(row++, "联结组别", params.connectionGroup,
                "频率(Hz)", QString::number(params.frequency_Hz),
                "connectionGroup", "frequency");
    item(row - 1, 5)->setText(QStringLiteral("仅50Hz；其他频率禁止计算"));
    if (m_designConnectionUi || sheetSelectionUi) {
        m_designConnectionRow = row - 1;
        m_designConnectionCombo = new QComboBox(this);
        m_designConnectionCombo->addItem(QStringLiteral("Dyn（Dyn11）"), QStringLiteral("Dyn11"));
        m_designConnectionCombo->addItem(QStringLiteral("Yyn（Yyn0）"), QStringLiteral("Yyn0"));
        m_designConnectionCombo->addItem(QStringLiteral("Dd（仅限非标手工指标）"), QStringLiteral("Dd"));
        m_designConnectionCombo->addItem(QStringLiteral("Yd（仅限非标手工指标）"), QStringLiteral("Yd"));
        QString selected = params.connectionGroup.trimmed();
        const auto type = connectionType(selected);
        if (type == SupportedConnection::Dyn11) selected = QStringLiteral("Dyn11");
        else if (type == SupportedConnection::Yyn0) selected = QStringLiteral("Yyn0");
        else if (type == SupportedConnection::Dd) selected = QStringLiteral("Dd");
        else if (type == SupportedConnection::Yd) selected = QStringLiteral("Yd");
        int selectedIndex = m_designConnectionCombo->findData(selected);
        if (selectedIndex < 0) {
            m_designConnectionCombo->addItem(selected + QStringLiteral("（未支持，需修改）"), selected);
            selectedIndex = m_designConnectionCombo->count() - 1;
        }
        for (int i = 0; i < m_designConnectionCombo->count(); ++i)
            m_designConnectionCombo->setItemData(i, m_designConnectionCombo->itemText(i), Qt::ToolTipRole);
        if (sheetSelectionUi && !m_designConnectionUi) {
            // Compact worksheet labels; authoritative values remain in UserRole.
            m_designConnectionCombo->setItemText(0, QStringLiteral("Dyn11"));
            m_designConnectionCombo->setItemText(1, QStringLiteral("Yyn0"));
            // This compact popup has no basic electrical preview rows.
            m_designConnectionCombo->setItemText(2, QStringLiteral("Dd（非标）"));
            m_designConnectionCombo->setItemText(3, QStringLiteral("Yd（非标）"));
            for (int i : {2, 3})
                m_designConnectionCombo->setItemData(i,
                    QStringLiteral("仅非标手工指标模式可完整计算及确认保存；无对应内置标准列，请核对各项性能指标。"), Qt::ToolTipRole);
        }
        m_designConnectionCombo->setCurrentIndex(selectedIndex);
        item(m_designConnectionRow, 2)->setText(selected);
        item(m_designConnectionRow, 2)->setFlags(item(m_designConnectionRow, 2)->flags() & ~Qt::ItemIsEditable);
        setCellWidget(m_designConnectionRow, 2, m_designConnectionCombo);
        auto* connectionCombo = m_designConnectionCombo;
        connect(connectionCombo, &QComboBox::currentIndexChanged, this, [this, connectionCombo](int) {
            if (m_loading || connectionCombo != m_designConnectionCombo) return;
            item(m_designConnectionRow, 2)->setText(connectionCombo->currentData().toString());
        });
    }
    if (m_designConnectionUi) {
        m_designPhaseRow = row;
        addParamRow(row++, QStringLiteral("高压额定相电压(V)"), QStringLiteral("待确认"),
                    QStringLiteral("低压相电压(V)"), QStringLiteral("待确认"));
        addParamRow(row++, QStringLiteral("高压相电流(A)"), QStringLiteral("待确认"),
                    QStringLiteral("低压相电流(A)"), QStringLiteral("待确认"));
        for (int r : {m_designPhaseRow, m_designPhaseRow + 1})
            for (int col : {2, 4}) item(r, col)->setFlags(item(r, col)->flags() & ~Qt::ItemIsEditable);
    }
    m_testVoltageRow = row;
    addParamRow(row++, QStringLiteral("高压试验电压(kV)"), QStringLiteral("需人工核对"),
                QStringLiteral("低压试验电压(kV)"), QStringLiteral("需人工核对"));
    for (int col : {2, 4})
        item(m_testVoltageRow, col)->setFlags(item(m_testVoltageRow, col)->flags() & ~Qt::ItemIsEditable);
    // 正向级数、负向级数和每级百分比在同一行独立输入。
    addParamRow(row, QStringLiteral("高压调压级电压"), QString());
    item(row, 5)->setText(QStringLiteral("（+级数，-级数）× 每级%"));
    auto *tapEditor = new QWidget(this);
    auto *tapLayout = new QHBoxLayout(tapEditor);
    tapLayout->setContentsMargins(2, 0, 2, 0);
    tapLayout->setSpacing(4);
    tapLayout->addWidget(new QLabel(QStringLiteral("（"), tapEditor));
    m_tapPlusSpin = new QSpinBox(tapEditor);
    m_tapPlusSpin->setRange(0, 10000);
    m_tapPlusSpin->setPrefix(QStringLiteral("+"));
    m_tapPlusSpin->setValue(params.hvTapPlusSteps);
    m_tapPlusSpin->setToolTip(QStringLiteral("正向调压级数"));
    tapLayout->addWidget(m_tapPlusSpin);
    tapLayout->addWidget(new QLabel(QStringLiteral("，"), tapEditor));
    m_tapMinusSpin = new QSpinBox(tapEditor);
    m_tapMinusSpin->setRange(0, 10000);
    m_tapMinusSpin->setPrefix(QStringLiteral("-"));
    m_tapMinusSpin->setValue(params.hvTapMinusSteps);
    m_tapMinusSpin->setToolTip(QStringLiteral("负向调压级数"));
    tapLayout->addWidget(m_tapMinusSpin);
    tapLayout->addWidget(new QLabel(QStringLiteral("）×"), tapEditor));
    m_tapStepSpin = new QDoubleSpinBox(tapEditor);
    m_tapStepSpin->setRange(0.001, 100.0);
    m_tapStepSpin->setDecimals(3);
    m_tapStepSpin->setSuffix(QStringLiteral("%"));
    m_tapStepSpin->setValue(params.hvTapVoltagePercent);
    m_tapStepSpin->setToolTip(QStringLiteral("每级调压电压百分比"));
    tapLayout->addWidget(m_tapStepSpin);
    tapLayout->addStretch();
    setSpan(row, 2, 1, 3);
    setCellWidget(row, 2, tapEditor);
    ++row;
    addInputRow(row++, "环境等级", params.environmentGrade,
                "计算折算温度(℃)", QString::number(params.calcRefTemp_C),
                "environmentGrade", "calcRefTemp");
    item(row - 1, 5)->setText(QStringLiteral("电阻固定折算75℃；环境等级仅记录"));
    addInputRow(row++, "最高环境温度(℃)", QString::number(params.maxAmbientTemp_C),
                "铁心截面计算方式", params.coreSectionCalcMethod,
                "maxAmbientTemp", {});
    item(row - 1, 5)->setText(QStringLiteral("环境温度未参与温升修正，仅记录"));
    addInputRow(row++, "最高海拔高度(m)", QString::number(params.maxAltitude_m),
                "负载损耗计算方式", params.loadLossCalcMethod,
                "maxAltitude", {});
    item(row - 1, 5)->setText(QStringLiteral("海拔未参与温升修正；当前为波纹油箱算法"));
    bindInput("efficiencyCalcMethod", 0, 4);

    // 二 性能指标
    addSectionRow(row++, QStringLiteral("二 性能指标"),
                  QStringLiteral("设计允许偏差:最小值(可选填)"), QStringLiteral("最大值(可选填)"));
    if (m_designConnectionUi || sheetSelectionUi) {
        m_standardModeRow = row;
        addParamRow(row++, QStringLiteral("指标来源模式"), QString(), QString(), QString());
        item(m_standardModeRow, 2)->setFlags(item(m_standardModeRow, 2)->flags() & ~Qt::ItemIsEditable);
        m_standardModeCombo = new QComboBox(this);
        auto *modeCombo = m_standardModeCombo;
        modeCombo->addItem(QStringLiteral("国标（内置表）"), int(TransformerParams::StandardMode::BuiltIn));
        modeCombo->addItem(QStringLiteral("非标（手工指标）"), int(TransformerParams::StandardMode::Custom));
        modeCombo->addItem(QStringLiteral("来源待确认"), int(TransformerParams::StandardMode::Unconfirmed));
        for (int i = 0; i < modeCombo->count(); ++i)
            modeCombo->setItemData(i, modeCombo->itemText(i), Qt::ToolTipRole);
        if (sheetSelectionUi && !m_designConnectionUi) {
            modeCombo->setItemText(0, QStringLiteral("国标"));
            modeCombo->setItemText(1, QStringLiteral("非标"));
            modeCombo->setItemText(2, QStringLiteral("待确认"));
        }
        modeCombo->setCurrentIndex(int(m_standardMode));
        setCellWidget(m_standardModeRow, 2, modeCombo);
        connect(modeCombo, &QComboBox::currentIndexChanged, this, [this, modeCombo](int index) {
            if (m_loading || modeCombo != m_standardModeCombo) return;
            const auto previous = m_standardMode;
            const auto next = TransformerParams::StandardMode(modeCombo->itemData(index).toInt());
            if (next == TransformerParams::StandardMode::BuiltIn) {
                const QString unavailable = standardUnavailableReason();
                if (previous == TransformerParams::StandardMode::Custom) {
                    const auto old = getParams();
                    if (std::isfinite(old.noLoadCurrentStd_pct) && old.noLoadCurrentStd_pct >= 0.0) {
                        m_baseParams.hasSavedCustomNoLoadCurrent = true;
                        m_baseParams.savedCustomNoLoadCurrent_pct = old.noLoadCurrentStd_pct;
                    }
                }
                if (!unavailable.isEmpty() || QMessageBox::question(this, QStringLiteral("切回国标"),
                    QStringLiteral("将按当前型号、容量和接法替换损耗指标；空载电流仅在“原单自动联动”时重新取值，“手工固定”时保留。阻抗和允许偏差保持手工值。是否继续？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                    if (!unavailable.isEmpty()) QMessageBox::warning(this, QStringLiteral("国标内置表不适用"), unavailable);
                    const QSignalBlocker blocker(modeCombo);
                    modeCombo->setCurrentIndex(int(previous));
                    return;
                }
            }
            m_standardMode = next;
            if (next == TransformerParams::StandardMode::Custom && !m_baseParams.hasManualNoLoadCurrent() && m_baseParams.hasSavedCustomNoLoadCurrent) {
                const auto ref = m_inputRefs.value(QStringLiteral("noLoadCurrentStd"), {-1, -1});
                if (auto *current = item(ref.first, ref.second)) {
                    const QSignalBlocker blocker(this);
                    current->setText(QString::number(m_baseParams.savedCustomNoLoadCurrent_pct, 'g', 12));
                }
            }
            m_lossStandardsManual = (next != TransformerParams::StandardMode::BuiltIn);
            m_lastLinkageKey = standardKey();
            applyModelLinkage();
            emit nonstandardAn3StateChanged();
        });
    }
    addInputRow(row++, "空载损耗标准值(W)", PerformanceCriteria::valueText(params.noLoadLossStd_W),
                "", PerformanceCriteria::valueText(params.noLoadLossMaxDev_pct),
                "noLoadLossStd", "noLoadLossMaxDev");
    addInputRow(row++, "负载损耗标准值(W)", PerformanceCriteria::valueText(params.loadLossStd_W),
                "", PerformanceCriteria::valueText(params.loadLossMaxDev_pct),
                "loadLossStd", "loadLossMaxDev");
    addInputRow(row++, "总损耗标准值(W)", PerformanceCriteria::valueText(params.totalLossStd_W),
                "", PerformanceCriteria::valueText(params.totalLossMaxDev_pct),
                "totalLossStd", "totalLossMaxDev");
    addInputRow(row++, "阻抗电压标准值(%)", PerformanceCriteria::valueText(params.impedanceVoltageStd_pct),
                PerformanceCriteria::valueText(params.impedanceVoltageMinDev_pct),
                PerformanceCriteria::valueText(params.impedanceVoltageMaxDev_pct),
                "impedanceVoltageStd", "impedanceVoltageMaxDev");
    bindInput("impedanceVoltageMinDev", row - 1, 3);
    addInputRow(row++, "空载电流标准值(%)", PerformanceCriteria::valueText(params.noLoadCurrentStd_pct),
                "", PerformanceCriteria::valueText(params.noLoadCurrentMaxDev_pct),
                "noLoadCurrentStd", "noLoadCurrentMaxDev");

    addInputRow(row++,QStringLiteral("空载电流取值"),m_baseParams.hasManualNoLoadCurrent()?QStringLiteral("manual"):QStringLiteral("automatic"),{},{},"noLoadCurrentSource",{});
    m_currentSourceRow=row-1;m_currentSourceCombo=new QComboBox(this);m_currentSourceCombo->setObjectName(QStringLiteral("noLoadCurrentSource"));
    m_currentSourceCombo->addItem(QStringLiteral("原单自动联动"),0);m_currentSourceCombo->addItem(QStringLiteral("手工固定"),1);
    m_currentSourceCombo->setCurrentIndex(m_baseParams.hasManualNoLoadCurrent()?1:0);setCellWidget(m_currentSourceRow,2,m_currentSourceCombo);
    m_currentSourceCombo->setToolTip(QStringLiteral("与国标/非标独立。自动沿用原单查表或AN3换算，不代表官方通用规定；手工保留O9，不因标准切换而覆盖。修改标准只影响偏差和校核，不改设计电流。"));
    connect(m_currentSourceCombo,&QComboBox::currentIndexChanged,this,[this](int index){if(m_loading||index<0)return;
        m_baseParams.noLoadCurrentSource=index==1?TransformerParams::NoLoadCurrentSource::Manual:TransformerParams::NoLoadCurrentSource::Automatic;
        {const QSignalBlocker blocker(this);item(m_currentSourceRow,2)->setText(index==1?QStringLiteral("manual"):QStringLiteral("automatic"));}
        applyModelLinkage();emit nonstandardAn3StateChanged();
    });
    // 三 温升限值：油顶层限值参与所有方案校核，不应随铁芯类型隐藏。
    addSectionRow(row++, QStringLiteral("三 温升限值"));
    addParamRow(row++, "油顶层温升限值(K)", PerformanceCriteria::valueText(params.oilTopTempRise_K));
    bindInput("oilTopTempRise", row - 1, 2);
    item(row - 1, 5)->setText(QStringLiteral("方案校核上限；≤0不校核"));
    item(row - 1, 2)->setToolTip(QStringLiteral(
        "比较计算结果中的油顶层温升，不是油面温升。仅影响方案校核和筛选，不改变温升计算公式；沿用原规则，限值≤0时不校核此项。"));
    addParamRow(row++, "高压线圈温升限值(K)", PerformanceCriteria::valueText(params.hvCoilTempRise_K));
    bindInput("hvCoilTempRise", row - 1, 2);
    addParamRow(row++, "低压线圈温升限值(K)", PerformanceCriteria::valueText(params.lvCoilTempRise_K));
    bindInput("lvCoilTempRise", row - 1, 2);

    // 四 铁芯参数（设计变量，初值取自 CalcInput）
    addSectionRow(row++, QStringLiteral("四 铁芯参数"));
    addInputRow(row++, "铁芯直径(mm)", QString::number(input.coreDiameter_mm, 'g', 15),
                "叠片系数", QString::number(input.stackFactor),
                "coreDiameter", "stackFactor");
    addInputRow(row++, "直线段长(mm)", QString::number(input.coreStraight_mm, 'g', 15),
                "椭圆角(°)", QString::number(input.ellipseAngle_deg),
                "coreStraight", "ellipseAngle");
    addInputRow(row++, "硅钢片牌号", QString(),
                "硅钢片厚(mm)", QString(), {}, "steelThickness");
    auto *gradeItem = item(row - 1, 2);
    gradeItem->setFlags(gradeItem->flags() & ~Qt::ItemIsEditable);
    auto *thicknessItem = item(row - 1, 4);
    thicknessItem->setFlags(thicknessItem->flags() & ~Qt::ItemIsEditable);
    thicknessItem->setToolTip(QStringLiteral("由硅钢片牌号前两位自动计算"));
    m_steelGradeCombo = new QComboBox(this);
    const QString currentGrade = input.steelGrade.trimmed();
    DesignDatabase &db = DesignDatabase::instance();
    const bool loaded = db.isLoaded() || db.load();
    int selectedIndex = -1;
    if (loaded) {
        for (const SteelCurve &curve : db.steelCurves()) {
            if (CalcInput::thicknessFromSteelGrade(curve.grade) <= 0.0) {
                continue;
            }
            const int index = m_steelGradeCombo->count();
            m_steelGradeCombo->addItem(curve.grade, curve.grade);
            if (curve.grade.compare(currentGrade, Qt::CaseInsensitive) == 0) {
                selectedIndex = index;
            }
        }
    }
    if (selectedIndex < 0) {
        const QString label = !loaded ? QStringLiteral("牌号数据加载失败")
            : currentGrade.isEmpty() ? QStringLiteral("请选择硅钢片牌号")
            : QStringLiteral("未收录：%1").arg(currentGrade);
        m_steelGradeCombo->insertItem(0, label, currentGrade);
        selectedIndex = 0;
    }
    m_steelGradeCombo->setCurrentIndex(selectedIndex);
    if (!loaded) {
        m_steelGradeCombo->setEnabled(false);
        m_steelGradeCombo->setToolTip(db.lastError());
    }
    setCellWidget(row - 1, 2, m_steelGradeCombo);
    connect(m_steelGradeCombo, &QComboBox::currentIndexChanged,
            this, [this](int) { updateSteelThickness(); });
    m_steelCurveRangeRow = row;
    addParamRow(row++, QStringLiteral("曲线磁密下限(T)"), QString(),
                QStringLiteral("曲线磁密上限(T)"), QString());
    for (int col : {2, 4})
        item(m_steelCurveRangeRow, col)->setFlags(item(m_steelCurveRangeRow, col)->flags() & ~Qt::ItemIsEditable);
    updateSteelThickness();
    addInputRow(row++, "心柱铁损工艺系数", QString::number(input.coreLossCraftCoef, 'g', 15),
                "铁轭铁损工艺系数", QString::number(input.yokeLossCraftCoef, 'g', 15),
                "coreLossCraftCoef", "yokeLossCraftCoef");
    item(row - 1, 5)->setText(QStringLiteral("分别作用；总铁损统一取整"));
    addInputRow(row++, QStringLiteral("原单单位铁损记录(W/kg)"), input.legacyUnitLossRecordText,
                QStringLiteral("原单旧工艺系数记录"), input.legacyCraftCoefRecordText,
                QStringLiteral("legacyUnitLossRecord"), QStringLiteral("legacyCraftCoefRecord"));
    const QString legacyCoreNote = QStringLiteral("原计算单H16/M16可选历史记录，空白表示未录入；不使用示例0.76/1.13，不跟随硅钢牌号自动覆盖。未参与计算，不替代当前查表单位铁损、心柱工艺系数或铁轭工艺系数；不得解释为当前引擎实际采用值。");
    item(row-1, 5)->setText(QStringLiteral("原单记录，未参与计算"));
    for (int col : {2,4,5}) item(row-1, col)->setToolTip(legacyCoreNote);
    addInputRow(row++, "接缝数", QString::number(input.seamCount), {}, {}, "seamCount", {});

    addInputRow(row++, "补充片1叠厚模式(F20)", QString(),
                "几何半叠厚(mm)", QString::number(input.yokePiece1Stack_mm, 'g', 15), {}, "yokePiece1Stack");
    item(row - 1, 2)->setFlags(item(row - 1, 2)->flags() & ~Qt::ItemIsEditable);
    m_yokePiece1ModeCombo = new QComboBox(this);
    m_yokePiece1ModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_yokePiece1ModeCombo->addItem(QStringLiteral("自动（计算单）"), true);
    m_yokePiece1ModeCombo->addItem(QStringLiteral("手工"), false);
    m_yokePiece1ModeCombo->setCurrentIndex(input.yokePiece1Auto ? 0 : 1);
    // 初次加载不把手工输入当作自动结果。
    item(row - 1, 4)->setData(Qt::UserRole, false);
    setCellWidget(row - 1, 2, m_yokePiece1ModeCombo);
    connect(m_yokePiece1ModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        updateYokePiece1();
        updateLvTurnsRecommendation();
    });

    if (m_designConnectionUi) {
        m_designCoreRow = row;
        addParamRow(row++, QStringLiteral("实际心柱截面(cm²)"), QStringLiteral("不可用"),
                    QStringLiteral("实际铁轭截面(cm²)"), QStringLiteral("不可用"));
        addParamRow(row++, QStringLiteral("实际心柱磁密(T)"), QStringLiteral("不可用"),
                    QStringLiteral("铁轭磁密(T)"), QStringLiteral("不可用"));
        addParamRow(row++, QStringLiteral("硅钢片重量(kg)"), QStringLiteral("不可用"),
                    QStringLiteral("空载损耗(W)"), QStringLiteral("不可用"));
        for (int r = m_designCoreRow; r < m_designCoreRow + 3; ++r)
            for (int col : {2, 4}) item(r, col)->setFlags(item(r, col)->flags() & ~Qt::ItemIsEditable);
        addLaminationPreview(row);
    }

    // 五 绕组参数（设计变量，初值取自 CalcInput）
    addSectionRow(row++, QStringLiteral("五 绕组参数"));
    m_recommendationRow = row;
    addInputRow(row++, "低压参考磁密(T)", QString::number(input.refFluxDens_T, 'g', 15),
                "推荐低压匝数", QString(), "refFluxDens", {});
    item(m_recommendationRow, 4)->setFlags(item(m_recommendationRow, 4)->flags() & ~Qt::ItemIsEditable);
    addParamRow(row, QStringLiteral("高压导线材料"), QString(),
                QStringLiteral("低压箔材料"), QString());
    m_hvMaterialCombo = new QComboBox(this);
    m_hvMaterialCombo->addItems({QStringLiteral("铜"), QStringLiteral("铝")});
    m_hvMaterialCombo->setCurrentIndex(input.hvCopperWire ? 0 : 1);
    m_lvMaterialCombo = new QComboBox(this);
    m_lvMaterialCombo->addItems({QStringLiteral("铜箔"), QStringLiteral("铝箔")});
    m_lvMaterialCombo->setCurrentIndex(input.lvCopperFoil ? 0 : 1);
    setCellWidget(row, 2, m_hvMaterialCombo);
    setCellWidget(row, 4, m_lvMaterialCombo);
    ++row;
    addInputRow(row++, "低压匝数", QString::number(input.lvTurns),
                "低压箔厚(mm)", QString::number(input.lvFoilThick_mm, 'g', 15),
                "lvTurns", "lvFoilThick");
    addInputRow(row++, "低压箔宽(mm)", QString::number(input.lvFoilWidth_mm, 'g', 15),
                "低压端绝缘(mm)", QString::number(input.lvEndInsul_mm),
                "lvFoilWidth", "lvEndInsul");
    addInputRow(row++, QStringLiteral("低压内连接排厚(mm)"), input.lvConnectionBarThicknessText,
                QStringLiteral("低压内连接排宽(mm)"), input.lvConnectionBarWidthText,
                QStringLiteral("lvConnectionBarThickness"), QStringLiteral("lvConnectionBarWidth"));
    const QString connectionBarNote = QStringLiteral("默认仅作工艺记录；选择辅助表设计截面时，必须填写有效内连接排厚宽，参与设计截面、磁密和空载电流。厚×宽−5仍仅为独立参考；默认厚6mm、宽50mm，取自原计算单，可修改。");
    item(row-1, 5)->setText(QStringLiteral("默认仅记录；辅助表设计截面时参与计算"));
    for (int col : {2,4,5}) item(row-1, col)->setToolTip(connectionBarNote);
    addParamRow(row, QStringLiteral("高压导线形状"), QString(), QStringLiteral("圆线裸直径(mm)"), QString());
    m_wireFormCombo = new QComboBox(this);
    m_wireFormCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_wireFormCombo->addItem(QStringLiteral("扁线"), false);
    m_wireFormCombo->addItem(QStringLiteral("圆线（计算单表）"), true);
    m_wireFormCombo->setCurrentIndex(input.isRoundHighVoltageWire() ? 1 : 0);
    m_roundWireSpecCombo = new QComboBox(this);
    m_roundWireSpecCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    const auto &wireDb = DesignDatabase::instance();
    for (const auto &spec : wireDb.wireSpecs()) {
        WireSpec checked;
        if (wireDb.roundWireSpec(spec.bareWidthMm, checked))
            m_roundWireSpecCombo->addItem(QString::number(spec.bareWidthMm, 'g', 15), spec.bareWidthMm);
    }
    WireSpec loadedSpec;
    int specIndex = wireDb.roundWireSpec(input.hvBareWidth_mm, loadedSpec)
        ? m_roundWireSpecCombo->findData(input.hvBareWidth_mm) : -1;
    if (input.isRoundHighVoltageWire() && specIndex < 0) {
        m_roundWireSpecCombo->insertItem(0, QStringLiteral("未收录：%1").arg(QString::number(input.hvBareWidth_mm, 'g', 17)), QVariant());
        specIndex = 0; // 不得把导入的无效圆线静默替换为可算规格。
    }
    if (specIndex < 0) specIndex = m_roundWireSpecCombo->findData(2.0);
    m_roundWireSpecCombo->setCurrentIndex(specIndex);
    setCellWidget(row, 2, m_wireFormCombo);
    setCellWidget(row, 4, m_roundWireSpecCombo);
    item(row, 5)->setText(QStringLiteral("仅计算单圆线表内规格；不插值、不向下取档、不越界"));
    ++row;
    addInputRow(row++, "高压裸线宽(mm)", QString::number(input.hvBareWidth_mm, 'g', input.isRoundHighVoltageWire() ? 17 : 15),
                "高压裸线厚(mm)", QString::number(input.hvBareThick_mm, 'g', input.isRoundHighVoltageWire() ? 17 : 15),
                "hvBareWidth", "hvBareThick");
    addInputRow(row++, "高压导线绝缘种类", QString(),
                "绝缘增厚(mm)", QString::number(input.hvWireInsulAdd_mm),
                {}, "hvWireInsulAdd");
    item(row - 1, 2)->setFlags(item(row - 1, 2)->flags() & ~Qt::ItemIsEditable);
    m_wireInsulationCombo = new QComboBox(this);
    m_wireInsulationCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    for (const QString &type : {QStringLiteral("QZB-2/130"), QStringLiteral("ZB-0.3"),
                               QStringLiteral("ZLB-0.3"), QStringLiteral("ZB-0.45"), QStringLiteral("ZLB-0.45")})
        m_wireInsulationCombo->addItem(type, type);
    m_wireInsulationCombo->addItem(QStringLiteral("自定义"), QStringLiteral("Custom"));
    m_wireInsulationCombo->addItem(QStringLiteral("圆线表（计算单缓存）"), QStringLiteral("RoundTable"));
    // 圆线表选项只由圆线模式启用，扁线不能选择该项。
    m_wireInsulationCombo->setItemData(m_wireInsulationCombo->count() - 1, 0, Qt::UserRole - 1);
    m_wireInsulationCombo->setCurrentIndex(m_wireInsulationCombo->findData(m_flatWireInsulation));
    setCellWidget(row - 1, 2, m_wireInsulationCombo);
    connect(m_wireInsulationCombo, &QComboBox::currentIndexChanged,
            this, [this](int) { updateWireInsulation(); });
    connect(m_wireFormCombo, &QComboBox::currentIndexChanged, this, [this](int) { updateWireForm(); });
    connect(m_roundWireSpecCombo, &QComboBox::currentIndexChanged, this, [this](int) { updateWireForm(); });
    updateWireForm();
    addInputRow(row++, "高压总层数 W12", QString::number(input.hvTurnsPerLayer),
                "层间绝缘厚(mm)", QString::number(input.hvLayerInsul_mm),
                "hvTurnsPerLayer", "hvLayerInsul");
    updateHvCoilHints();
    addInputRow(row++, "高压并绕根数", QString::number(input.hvParallelCount),
                "高压叠绕根数", QString::number(input.hvStackCount),
                "hvParallelCount", "hvStackCount");
    item(row - 1, 5)->setText(QStringLiteral("总有效截面=单根截面×并绕×叠绕"));

    // 六 主空道（设计变量，初值取自 CalcInput）
    addSectionRow(row++, QStringLiteral("六 主空道"));
    // Search widths must survive an unchanged load/collect at a strict craft minimum.
    addInputRow(row++, "主空道宽(mm)", QString::number(input.mainDuctWidth_mm, 'g', 17),
                "纸板厚(mm)", QString::number(input.mainDuctInsul_mm),
                "mainDuctWidth", "mainDuctInsul");
    item(row - 1, 5)->setText(QStringLiteral("第二油道AG43自动联动：高压>12kV为5mm，否则0；>24kV另加1.5+5mm"));

    // 七~十 高级参数（仅专业模式）
    if (proMode) {
        addProModeSections(row, input);
    }
    if (m_designConnectionUi) {
        connect(m_productModelEdit, &QLineEdit::textEdited, this, [this]() { updateDesignConnection(); scheduleDesignCore(); });
        for (auto *combo : {m_designConnectionCombo, m_standardModeCombo, m_steelGradeCombo,
                            m_yokePiece1ModeCombo, m_hvMaterialCombo, m_lvMaterialCombo,
                            m_wireInsulationCombo, m_wireFormCombo, m_roundWireSpecCombo}) {
            if (combo) connect(combo, &QComboBox::currentIndexChanged, this, [this](int) { scheduleDesignCore(); });
        }
        for (auto *spin : {m_tapPlusSpin, m_tapMinusSpin}) {
            connect(spin, &QSpinBox::valueChanged, this, [this]() { updateDesignConnection(); scheduleDesignCore(); });
            connect(spin->findChild<QLineEdit *>(), &QLineEdit::textEdited, this, [this]() { updateDesignConnection(); scheduleDesignCore(); });
        }
        connect(m_tapStepSpin, &QDoubleSpinBox::valueChanged, this, [this]() { updateDesignConnection(); scheduleDesignCore(); });
        connect(m_tapStepSpin->findChild<QLineEdit *>(), &QLineEdit::textEdited, this, [this]() { updateDesignConnection(); scheduleDesignCore(); });
    }
    m_loading = false;
    // 导入的明确损耗指标没有会话来源键，视作本规格手工值，不覆盖。
    if (m_lossStandardsManual && m_lastLinkageKey.isEmpty())
        m_lastLinkageKey = standardKey();
    applyModelLinkage();
}

void ParamTableWidget::updateHvCoilHints()
{
    const auto ref = m_inputRefs.constFind(QStringLiteral("hvTurnsPerLayer"));
    if (ref == m_inputRefs.constEnd()) return;
    const QSignalBlocker blocker(this);
    const QString detail = HvCoilFormNotes::layerNote();
    for (int col : {1, 2, 5}) {
        if (auto *cell = item(ref->first, col)) cell->setToolTip(detail);
    }
    if (auto *note = item(ref->first, 5)) {
        note->setText(m_config.hvCoilStructure == StructureConfig::TwoSegCylinder
            ? QStringLiteral("分段：两段串联；五组油道均启用时可计算")
            : QStringLiteral("W12总层数；Y9自动计算；分段工艺口径见悬停提示"));
    }
}

// 从表格设计变量节读回 CalcInput：空值/非法值保持原字段不变
void ParamTableWidget::saveToInput(CalcInput &input) const
{
    input.coreShape = m_config.coreShape;
    input.yokeShape = m_config.yokeShape;
    input.hasPerformanceCriteria = true;
    input.performanceCriteria = getParams();
    if (m_hvMaterialCombo && m_lvMaterialCombo) {
        input.hvCopperWire = (m_hvMaterialCombo->currentIndex() == 0);
        input.lvCopperFoil = (m_lvMaterialCombo->currentIndex() == 0);
    }
    const auto connection = m_inputRefs.constFind(QStringLiteral("connectionGroup"));
    if (connection != m_inputRefs.constEnd() && item(connection->first, connection->second)) {
        const SupportedConnection type = connectionType(item(connection->first, connection->second)->text());
        if (type != SupportedConnection::Invalid) {
            input.hvDeltaConnected = (type == SupportedConnection::Dyn11 || type == SupportedConnection::Dd);
            input.lvStarConnected = (type == SupportedConnection::Dyn11 || type == SupportedConnection::Yyn0);
        }
    }
    if (m_tapPlusSpin && m_tapMinusSpin && m_tapStepSpin) {
        input.hvTapPlusSteps = m_tapPlusSpin->value();
        input.hvTapMinusSteps = m_tapMinusSpin->value();
        input.hvTapStep_pct = m_tapStepSpin->value();
        input.hvTapMax_pct = input.hvTapPlusSteps * input.hvTapStep_pct;
        input.hvTapMin_pct = -input.hvTapMinusSteps * input.hvTapStep_pct;
    }
    const auto cellText = [this](const QString &key) -> QString {
        const auto it = m_inputRefs.constFind(key);
        if (it == m_inputRefs.constEnd() || !item(it->first, it->second)) {
            return QString();
        }
        return item(it->first, it->second)->text().trimmed();
    };
    const auto setDouble = [&cellText](const QString &key, double &dst) {
        bool ok = false;
        const double v = cellText(key).toDouble(&ok);
        if (ok) {
            dst = v;
        }
    };
    const auto setInt = [&cellText](const QString &key, int &dst) {
        bool ok = false;
        const int v = cellText(key).toInt(&ok);
        if (ok) {
            dst = v;
        }
    };

    // 铁芯
    setDouble("coreDiameter", input.coreDiameter_mm);
    setDouble("stackFactor", input.stackFactor);
    setDouble("coreStraight", input.coreStraight_mm);
    setDouble("ellipseAngle", input.ellipseAngle_deg);
    setDouble("coreLossCraftCoef", input.coreLossCraftCoef);
    setDouble("yokeLossCraftCoef", input.yokeLossCraftCoef);
    input.legacyUnitLossRecordText = cellText(QStringLiteral("legacyUnitLossRecord"));
    input.legacyCraftCoefRecordText = cellText(QStringLiteral("legacyCraftCoefRecord"));
    setInt("seamCount", input.seamCount);
    const QString grade = selectedSteelGrade();
    input.steelGrade = grade;
    input.steelThickness_mm = CalcInput::thicknessFromSteelGrade(grade);

    // 低压绕组
    setInt("lvTurns", input.lvTurns);
    setDouble("refFluxDens", input.refFluxDens_T);
    setDouble("lvFoilThick", input.lvFoilThick_mm);
    setDouble("lvFoilWidth", input.lvFoilWidth_mm);
    setDouble("lvEndInsul", input.lvEndInsul_mm);
    input.lvConnectionBarThicknessText = cellText(QStringLiteral("lvConnectionBarThickness"));
    input.lvConnectionBarWidthText = cellText(QStringLiteral("lvConnectionBarWidth"));

    // 高压绕组
    setDouble("hvBareWidth", input.hvBareWidth_mm);
    setDouble("hvBareThick", input.hvBareThick_mm);
    if (m_wireFormCombo && m_wireFormCombo->currentData().toBool()) {
        const double diameter = m_roundWireSpecCombo && m_roundWireSpecCombo->currentData().isValid()
            ? m_roundWireSpecCombo->currentData().toDouble() : m_unlistedRoundDiameter_mm;
        // 保存/切换普通专业模式也走此路径；无效规格不能因单元格显示舍入变成有效档。
        input.hvBareWidth_mm = input.hvBareThick_mm = diameter;
    }
    setInt("hvTurnsPerLayer", input.hvTurnsPerLayer);
    setDouble("hvLayerInsul", input.hvLayerInsul_mm);
    setInt("hvParallelCount", input.hvParallelCount);
    setInt("hvStackCount", input.hvStackCount);
    setDouble("hvWireInsulAdd", input.hvWireInsulAdd_mm);
    if (m_wireInsulationCombo) {
        input.hvWireInsulation = m_wireInsulationCombo->currentData().toString();
        const double preset = input.isRoundHighVoltageWire() ? -1.0 : CalcInput::insulationIncrement(input.hvWireInsulation);
        if (preset >= 0.0) input.hvWireInsulAdd_mm = preset;
    }

    // 主空道
    setDouble("mainDuctWidth", input.mainDuctWidth_mm);
    setDouble("mainDuctInsul", input.mainDuctInsul_mm);

    // ---- 以下为专业模式高级参数（未绑定时保持原值）----

    // 七 铁芯工艺
    input.yokePiece1Auto = m_yokePiece1ModeCombo ? m_yokePiece1ModeCombo->currentData().toBool()
                                               : m_recommendationBaseInput.yokePiece1Auto;
    if (input.yokePiece1Auto) input.yokePiece1Stack_mm = m_manualYokePiece1_mm;
    else setDouble("yokePiece1Stack", input.yokePiece1Stack_mm);
    setDouble("yokePiece2Stack", input.yokePiece2Stack_mm);
    setDouble("yokePiece3Stack", input.yokePiece3Stack_mm);
    setDouble("yokeWidenTo", input.yokeWidenTo_mm);
    setInt("yokeWidenStages", input.yokeWidenStages);
    for (int i = 0; i < 20; ++i) {
        if(i>=16) input.yokeSteps_mm[i]=m_recommendationBaseInput.yokeSteps_mm[i];
        setDouble(QStringLiteral("yokeStep_%1").arg(i), input.yokeSteps_mm[i]);
        input.coreSheetCountAdjust[i]=m_recommendationBaseInput.coreSheetCountAdjust[i];
        setDouble(QStringLiteral("coreSheetAdjust_%1").arg(i), input.coreSheetCountAdjust[i]);
        input.coreWidthOffsets_mm[i]=m_recommendationBaseInput.coreWidthOffsets_mm[i];
        setDouble(QStringLiteral("coreWidthOffset_%1").arg(i),input.coreWidthOffsets_mm[i]);
    }

    // 八 绕组工艺（油道与绝缘细节）
    setInt("lvLayerInsulCount", input.lvLayerInsulCount);
    setDouble("lvLayerInsul_mm", input.lvLayerInsul_mm);
    for (int i = 0; i < 5; ++i) {
        setDouble(QStringLiteral("hvDuctW_%1").arg(i), input.hvDuctWidthSide[i]);
        setDouble(QStringLiteral("hvDuctH_%1").arg(i), input.hvDuctHeightSide[i]);
        setDouble(QStringLiteral("lvDuctW_%1").arg(i), input.lvDuctWidthSide[i]);
        setDouble(QStringLiteral("lvDuctH_%1").arg(i), input.lvDuctHeightSide[i]);
    }

    // 九 损耗系数
    setDouble("strayLossFactor", input.strayLossFactor);
    setDouble("leadLoss", input.leadLoss_W);
    setDouble("lvExtraLoss", input.lvExtraLoss_W);

    // 十 油箱与结构
    setDouble("oilTopCorrection", input.oilTopCorrection_K);
    setDouble("tankBottomOil", input.tankBottomOil_mm);
    setDouble("tankSideClear", input.tankSideClear_mm);
    setDouble("tankEndClear", input.tankEndClear_mm);
    setDouble("tankFoot", input.tankFoot_mm);
    setInt("waveDepth", input.waveDepth_mm);
    setInt("waveHeight", input.waveHeight_mm);
    setInt("wavePitch", input.wavePitch_mm);
    setDouble("phaseGapBase", input.phaseGapBase_mm);
    input.centerDistanceAllowance_mm=m_recommendationBaseInput.centerDistanceAllowance_mm;
    input.hvLayerInsulCalcFactor=m_recommendationBaseInput.hvLayerInsulCalcFactor;
    input.hvMaxTurnsAdjust=m_recommendationBaseInput.hvMaxTurnsAdjust;
    input.activePartHvWeightMode=m_recommendationBaseInput.activePartHvWeightMode;
    setDouble("activePartHvWeightMode",input.activePartHvWeightMode);
    input.thermalFormulaMode=m_recommendationBaseInput.thermalFormulaMode;input.lvThermalExtraMode=m_recommendationBaseInput.lvThermalExtraMode;
    setDouble("thermalFormulaMode",input.thermalFormulaMode);setDouble("lvThermalExtraMode",input.lvThermalExtraMode);
    input.designAreaMode=m_recommendationBaseInput.designAreaMode;setDouble("designAreaMode",input.designAreaMode);
    input.unitIronLossMode=m_recommendationBaseInput.unitIronLossMode;
    input.manualCoreLossPerKg_W=m_recommendationBaseInput.manualCoreLossPerKg_W;
    input.manualYokeLossPerKg_W=m_recommendationBaseInput.manualYokeLossPerKg_W;
    setDouble("unitIronLossMode",input.unitIronLossMode);setDouble("manualCoreLossPerKg",input.manualCoreLossPerKg_W);setDouble("manualYokeLossPerKg",input.manualYokeLossPerKg_W);
    input.hvAxialAllowance_mm=m_recommendationBaseInput.hvAxialAllowance_mm;
    input.hvDuctLayerRule=m_recommendationBaseInput.hvDuctLayerRule;setDouble("hvDuctLayerRule",input.hvDuctLayerRule);
    input.impedanceOutputMode=m_recommendationBaseInput.impedanceOutputMode;
    input.impedanceCalibrationFactor=m_recommendationBaseInput.impedanceCalibrationFactor;
    setDouble("hvAxialAllowance",input.hvAxialAllowance_mm);setDouble("impedanceOutputMode",input.impedanceOutputMode);setDouble("impedanceCalibrationFactor",input.impedanceCalibrationFactor);
    setDouble("hvMaxTurnsAdjust",input.hvMaxTurnsAdjust);
    setDouble("centerDistanceAllowance",input.centerDistanceAllowance_mm);
    setDouble("hvLayerInsulCalcFactor",input.hvLayerInsulCalcFactor);
    input.tankRimHighCapacity_mm=m_recommendationBaseInput.tankRimHighCapacity_mm;
    input.tankWallThicknessOverride_mm=m_recommendationBaseInput.tankWallThicknessOverride_mm;
    input.tankBottomThicknessAdjust_mm=m_recommendationBaseInput.tankBottomThicknessAdjust_mm;
    input.conservatorWeight_kg=m_recommendationBaseInput.conservatorWeight_kg;
    input.waveLongCountAdjust=m_recommendationBaseInput.waveLongCountAdjust;
    setDouble("tankRimHighCapacity",input.tankRimHighCapacity_mm);setDouble("tankWallThicknessOverride",input.tankWallThicknessOverride_mm);
    setDouble("tankBottomThicknessAdjust",input.tankBottomThicknessAdjust_mm);setDouble("conservatorWeight",input.conservatorWeight_kg);setDouble("waveLongCountAdjust",input.waveLongCountAdjust);
}

// 专业模式追加的高级参数节（七~十）：铁芯工艺 / 绕组油道 / 损耗系数 / 油箱结构，
// 数组类参数（轭阶梯 16 级、轴向油道各 5 位）平铺为逐行展示
void ParamTableWidget::addProModeSections(int &row, const CalcInput &input)
{
    // 七 铁芯工艺
    addSectionRow(row++, QStringLiteral("七 铁芯工艺（高级）"), {}, {}, true);
    addInputRow(row++,QStringLiteral("设计截面来源"),QString::number(input.designAreaMode),{},{},"designAreaMode",{});
    const int areaRow=row-1;auto *areaMode=new QComboBox(this);areaMode->setObjectName(QStringLiteral("designAreaMode"));
    areaMode->addItem(QStringLiteral("几何叠厚"),0);areaMode->addItem(QStringLiteral("辅助表片数"),1);areaMode->setCurrentIndex(input.designAreaMode==0?0:(input.designAreaMode==1?1:-1));setCellWidget(areaRow,2,areaMode);
    const QString areaNote=QStringLiteral("按原表F14来源选择；辅助表片数按内连接排厚宽、弦高和片数进位动态求解，改变设计磁密及空载电流，不直接替换重量表实际截面。辅助模式必须填写有效内连接排厚宽。默认保留几何叠厚。");
    areaMode->setToolTip(areaNote);item(areaRow,5)->setToolTip(areaNote);item(areaRow,5)->setText(QStringLiteral("辅助表需内连接排尺寸；重量与实际截面独立"));
    connect(areaMode,&QComboBox::currentIndexChanged,this,[this,areaMode,areaRow](int index){if(index>=0){item(areaRow,2)->setText(QString::number(areaMode->currentData().toInt()));scheduleDesignCore();}});
    addInputRow(row++,QStringLiteral("单位铁损来源"),QString::number(input.unitIronLossMode),QStringLiteral("心柱铁损(W/kg)"),QString::number(input.manualCoreLossPerKg_W,'g',15),"unitIronLossMode","manualCoreLossPerKg");
    const int lossModeRow=row-1;
    item(lossModeRow,5)->setText(QStringLiteral("曲线自动取值；手工模式须填两项正数"));
    addInputRow(row++,QStringLiteral("铁轭铁损(W/kg)"),QString::number(input.manualYokeLossPerKg_W,'g',15),{},{},"manualYokeLossPerKg",{});
    const int yokeLossRow=row-1;
    item(yokeLossRow,5)->setText(QStringLiteral("仅手工模式参与；原单旧记录栏仍只记录"));
    auto *lossMode=new QComboBox(this);lossMode->setObjectName(QStringLiteral("unitIronLossMode"));
    lossMode->addItem(QStringLiteral("曲线查表"),0);lossMode->addItem(QStringLiteral("手工指定"),1);
    const QString unitLossNote=QStringLiteral("曲线模式下手工栏仅保留，不参与计算。手工模式用于按原表指定单位铁损复算；磁密或材料变化时须重新确认数值，或切回曲线模式联动。此值不是DNN实测数据。");
    lossMode->setToolTip(unitLossNote);item(lossModeRow,4)->setToolTip(unitLossNote);item(yokeLossRow,2)->setToolTip(unitLossNote);
    lossMode->setCurrentIndex(input.unitIronLossMode==0?0:(input.unitIronLossMode==1?1:-1));setCellWidget(lossModeRow,2,lossMode);
    const auto updateLossMode=[this,lossMode,lossModeRow,yokeLossRow](int index){
        if(index>=0)item(lossModeRow,2)->setText(QString::number(lossMode->currentData().toInt()));
        scheduleDesignCore();
    };
    connect(lossMode,&QComboBox::currentIndexChanged,this,updateLossMode);updateLossMode(lossMode->currentIndex());
    addInputRow(row++, "轭片放大片宽(mm)", QString::number(input.yokeWidenTo_mm),
                "", "", "yokeWidenTo", {});
    addInputRow(row++, "补充片2手工半叠厚(mm)", QString::number(input.yokePiece2Stack_mm),
                "轭片放大级数", QString::number(input.yokeWidenStages),
                "yokePiece2Stack", "yokeWidenStages");
    addInputRow(row++, "补充片3手工半叠厚(mm)", QString::number(input.yokePiece3Stack_mm),
                "", "",
                "yokePiece3Stack", {});
    for (int i = 0; i < 20; ++i) {
        addInputRow(row++, QStringLiteral("轭阶梯 %1 外伸半宽(mm)").arg(i + 1),
                    QString::number(input.yokeSteps_mm[i]),
                    QStringLiteral("第%1级片数修正(片)").arg(i+1),
                    QString::number(input.coreSheetCountAdjust[i], 'g', 15),
                    QStringLiteral("yokeStep_%1").arg(i), QStringLiteral("coreSheetAdjust_%1").arg(i));
        const QString note=i<16
            ? QStringLiteral("Sheet1 L%1：在按当前叠厚、接缝数及片厚取整的基础片数上加此整数；0为不修正。轭阶梯-1表示自动。实际采用值见补充参数的计算说明页。").arg(i+5)
            : QStringLiteral("第%1级为备用工艺参数：可编辑并随方案保存，当前没有对应片宽、叠厚几何数据，不参与计算；不对应原计算单单元格。").arg(i+1);
        item(row-1,4)->setToolTip(note);
        item(row-1,2)->setToolTip(note);
        item(row-1,5)->setText(i<16 ? QStringLiteral("L%1：基础片数 + 修正；轭阶梯-1=自动").arg(i+5)
                                                   : QStringLiteral("备用：仅保存，未参与计算"));
        item(row-1,5)->setToolTip(note);
    }

    addSectionRow(row++,QStringLiteral("片宽查表修正(mm)"),{}, {},true);
    const QStringList widthAddresses={"G17","H17","I17","J17","K17","L17","M17","N17","O17","Q17","S17","F19","G19","H19","I19"};
    for(int i=0;i<20;i+=2){
        addInputRow(row++,QStringLiteral("第%1级片宽修正(mm)").arg(i+1),QString::number(input.coreWidthOffsets_mm[i],'g',15),
                    QStringLiteral("第%1级片宽修正(mm)").arg(i+2),QString::number(input.coreWidthOffsets_mm[i+1],'g',15),
                    QStringLiteral("coreWidthOffset_%1").arg(i),QStringLiteral("coreWidthOffset_%1").arg(i+1));
        const auto address=[&](int j){return j<15 ? widthAddresses[j] : QStringLiteral("备用");};
        const auto note=QStringLiteral("%1 / %2：实际片宽=当前尺寸查表基数+本修正；不是固定片宽。无几何级次仅保存备用。").arg(address(i),address(i+1));
        item(row-1,5)->setText(QStringLiteral("%1/%2：查表基数＋修正(mm)").arg(address(i),address(i+1)));
        item(row-1,5)->setToolTip(note);
        item(row-1,2)->setToolTip(note);item(row-1,4)->setToolTip(note);
    }
    // 八 绕组工艺（油道与绝缘细节）
    addSectionRow(row++, QStringLiteral("八 绕组工艺（高级）"), {}, {}, true);
    addInputRow(row++, QStringLiteral("油道宽/高含义"), QStringLiteral("相间侧/端部侧"),
                QStringLiteral("各侧停用值"), QStringLiteral("0"), {}, {});
    for (int col : {2, 4})
        item(row - 1, col)->setFlags(item(row - 1, col)->flags() & ~Qt::ItemIsEditable);
    item(row - 1, 5)->setText(QStringLiteral("两侧独立，尺寸非负；异常布局需核对"));
    item(row - 1, 5)->setToolTip(QStringLiteral("原表按相间侧和端部侧分别判0；可单侧停用，不自动配对、重排或改变公式。单侧启用或端部不连续布局仅提示需人工核对。"));
    addInputRow(row++,QStringLiteral("层绝缘计算系数"),QString::number(input.hvLayerInsulCalcFactor,'g',15),
                QStringLiteral("最大匝数修正(匝)"),QString::number(input.hvMaxTurnsAdjust,'g',15),
                QStringLiteral("hvLayerInsulCalcFactor"),QStringLiteral("hvMaxTurnsAdjust"));
    const QString factorNote=QStringLiteral("高压层间绝缘张数公式的分母系数；原表分别为4900、5250、5300，需按所用计算单确认，非自动标准选择。影响绝缘张数、绕组辐向及中心距。");
    item(row-1,5)->setText(QStringLiteral("绝缘系数与最大匝数修正按原表配置"));
    item(row-1,5)->setToolTip(factorNote);item(row-1,2)->setToolTip(factorNote);
    const QString turnNote=QStringLiteral("高压V8按最大相电压/匝电压取整后，再加此整数修正。默认0；原表明确减1匝时填-1。联动导线长度、重量和相关损耗；不改变额定/最小匝数，也不自动判定制造合格。");
    item(row-1,3)->setToolTip(turnNote);item(row-1,4)->setToolTip(turnNote);
    addInputRow(row++,QStringLiteral("高压油道层序"),QString::number(input.hvDuctLayerRule),QStringLiteral("高压轴向附加(mm)"),QString::number(input.hvAxialAllowance_mm),"hvDuctLayerRule","hvAxialAllowance");
    item(row-1,5)->setText(QStringLiteral("三档/五档按原表；高度取整后再加附加量"));
    const int ductRuleRow=row-1;auto *ductRule=new QComboBox(this);ductRule->setObjectName(QStringLiteral("hvDuctLayerRule"));
    ductRule->addItem(QStringLiteral("五档规则"),0);ductRule->addItem(QStringLiteral("三档规则"),1);
    ductRule->setCurrentIndex(input.hvDuctLayerRule==0?0:(input.hvDuctLayerRule==1?1:-1));setCellWidget(ductRuleRow,2,ductRule);
    connect(ductRule,&QComboBox::currentIndexChanged,this,[this,ductRule,ductRuleRow](int index){if(index>=0){item(ductRuleRow,2)->setText(QString::number(ductRule->currentData().toInt()));scheduleDesignCore();}});
    addInputRow(row++, "低压层间绝缘层数", QString::number(input.lvLayerInsulCount),
                "", "", "lvLayerInsulCount", {});
    addInputRow(row++, "低压层间绝缘厚(mm)", QString::number(input.lvLayerInsul_mm),
                "", "",
                "lvLayerInsul_mm", {});
    for (int i = 0; i < 5; ++i) {
        addInputRow(row++, QStringLiteral("高压油道宽 %1(mm)").arg(i + 1),
                    QString::number(input.hvDuctWidthSide[i]),
                    QStringLiteral("高压油道高 %1(mm)").arg(i + 1),
                    QString::number(input.hvDuctHeightSide[i]),
                    QStringLiteral("hvDuctW_%1").arg(i),
                    QStringLiteral("hvDuctH_%1").arg(i));
    }
    for (int i = 0; i < 5; ++i) {
        addInputRow(row++, QStringLiteral("低压油道宽 %1(mm)").arg(i + 1),
                    QString::number(input.lvDuctWidthSide[i]),
                    QStringLiteral("低压油道高 %1(mm)").arg(i + 1),
                    QString::number(input.lvDuctHeightSide[i]),
                    QStringLiteral("lvDuctW_%1").arg(i),
                    QStringLiteral("lvDuctH_%1").arg(i));
    }

    // 九 损耗系数
    addSectionRow(row++, QStringLiteral("九 损耗系数（高级）"), {}, {}, true);
    addInputRow(row++, "杂散损耗系数", QString::number(input.strayLossFactor),
                "引线损耗(W，未参与)", QString::number(input.leadLoss_W),
                "strayLossFactor", "leadLoss");
    const QString leadLossNote = QStringLiteral("引线损耗仅编辑、保存与回显，未计入负载损耗、温升或寻优损耗判定；工艺定义确认后再接入算法。原表J10为杂散系数，不是此引线损耗值。");
    item(row - 1, 3)->setToolTip(leadLossNote);
    item(row - 1, 4)->setToolTip(leadLossNote);
    item(row - 1, 5)->setText(QStringLiteral("引线损耗仅记录，未参与计算"));
    item(row - 1, 5)->setToolTip(leadLossNote);
    addInputRow(row++, "低压附加损耗(W)", QString::number(input.lvExtraLoss_W),
                "", "",
                "lvExtraLoss", {});
    const QString lvExtraLossNote = QStringLiteral("扁线计入负载损耗，圆线不计入负载损耗；温升选择“跟随损耗附加”时使用此值，选择“按箔材公式”时单独计算热附加项，不重复加入负载损耗。");
    item(row - 1, 1)->setToolTip(lvExtraLossNote);
    item(row - 1, 2)->setToolTip(lvExtraLossNote);
    item(row - 1, 5)->setText(QStringLiteral("扁线计入负载损耗；温升取值见低压热附加来源"));
    item(row - 1, 5)->setToolTip(lvExtraLossNote);

    addInputRow(row++,QStringLiteral("温升公式版本"),QString::number(input.thermalFormulaMode),QStringLiteral("低压热附加来源"),QString::number(input.lvThermalExtraMode),"thermalFormulaMode","lvThermalExtraMode");
    const int thermalRow=row-1;
    const QString thermalNote=QStringLiteral("五档保留原默认公式；三档使用原表散热周长及低压层间末项1.5，第四、五档须为0。热附加可跟随低压附加损耗或按箔材公式计算；箔材公式只影响温升，不重复计入负载损耗。");
    item(thermalRow,5)->setText(QStringLiteral("按原表选择；热附加与负载损耗分别计算"));item(thermalRow,5)->setToolTip(thermalNote);
    const auto addThermalCombo=[this,thermalRow,thermalNote](const char *key,int column,double value,const QString &zero,const QString &one){
        auto *combo=new QComboBox(this);combo->setObjectName(QString::fromLatin1(key));combo->addItem(zero,0);combo->addItem(one,1);
        combo->setCurrentIndex(value==0?0:(value==1?1:-1));combo->setToolTip(thermalNote);setCellWidget(thermalRow,column,combo);
        connect(combo,&QComboBox::currentIndexChanged,this,[this,combo,thermalRow,column](int index){if(index>=0){item(thermalRow,column)->setText(QString::number(combo->currentData().toInt()));scheduleDesignCore();}});
    };
    addThermalCombo("thermalFormulaMode",2,input.thermalFormulaMode,QStringLiteral("五档原表"),QStringLiteral("三档原表"));
    addThermalCombo("lvThermalExtraMode",4,input.lvThermalExtraMode,QStringLiteral("跟随损耗附加"),QStringLiteral("按箔材公式"));
    // 十 油箱与结构
    addInputRow(row++,QStringLiteral("阻抗输出口径"),QString::number(input.impedanceOutputMode),QStringLiteral("阻抗校正系数"),QString::number(input.impedanceCalibrationFactor),"impedanceOutputMode","impedanceCalibrationFactor");
    item(row-1,5)->setText(QStringLiteral("直接Q35或Q35÷Kx×系数；按原表选择"));
    const int impedanceRow=row-1;auto *impedanceMode=new QComboBox(this);impedanceMode->setObjectName(QStringLiteral("impedanceOutputMode"));
    impedanceMode->addItem(QStringLiteral("直接合成值"),0);impedanceMode->addItem(QStringLiteral("按Kx和系数修正"),1);
    impedanceMode->setCurrentIndex(input.impedanceOutputMode==0?0:(input.impedanceOutputMode==1?1:-1));setCellWidget(impedanceRow,2,impedanceMode);
    connect(impedanceMode,&QComboBox::currentIndexChanged,this,[this,impedanceMode,impedanceRow](int index){if(index>=0){item(impedanceRow,2)->setText(QString::number(impedanceMode->currentData().toInt()));scheduleDesignCore();}});
    addSectionRow(row++, QStringLiteral("十 油箱与结构（高级）"), {}, {}, true);
    addInputRow(row++,QStringLiteral("中心距附加量(mm)"),QString::number(input.centerDistanceAllowance_mm,'g',15),
                {},{},QStringLiteral("centerDistanceAllowance"),{});
    const QString centerNote=QStringLiteral("绕组尺寸链及电压间隙按5mm向上进位后再加的量；原表分别为5、1、4mm。不是相间距基础，也不是固定中心距。改变后联动铁轭重量和油箱尺寸。");
    item(row-1,5)->setText(QStringLiteral("中心距进位后附加；不同于相间距基础"));
    item(row-1,5)->setToolTip(centerNote);item(row-1,2)->setToolTip(centerNote);
    addInputRow(row++,QStringLiteral("630档箱沿高(mm)"),QString::number(input.tankRimHighCapacity_mm),QStringLiteral("箱壁指定厚(mm)"),QString::number(input.tankWallThicknessOverride_mm),"tankRimHighCapacity","tankWallThicknessOverride");
    item(row-1,5)->setText(QStringLiteral("容量≥630使用此箱沿高；箱壁0为自动"));
    addInputRow(row++,QStringLiteral("箱底厚度修正(mm)"),QString::number(input.tankBottomThicknessAdjust_mm),QStringLiteral("储油柜重(kg)"),QString::number(input.conservatorWeight_kg),"tankBottomThicknessAdjust","conservatorWeight");
    item(row-1,5)->setText(QStringLiteral("箱底自动值加修正；D18手工重计入C19"));
    item(row-1,4)->setToolTip(QStringLiteral("仅原表D18手工重量；不是自动选型，不含C23储油柜充油重量。"));
    addInputRow(row++,QStringLiteral("长边波纹数修正"),QString::number(input.waveLongCountAdjust),QStringLiteral("器身高压线重口径"),QString::number(input.activePartHvWeightMode),"waveLongCountAdjust","activePartHvWeightMode");
    item(row-1,5)->setText(QStringLiteral("片数修正与器身线重口径均按原表配置"));
    const int weightModeRow=row-1;
    auto *weightMode=new QComboBox(this);weightMode->setObjectName(QStringLiteral("activePartHvWeightMode"));
    weightMode->addItem(QStringLiteral("绝缘导线重"),0);weightMode->addItem(QStringLiteral("裸导线重"),1);
    weightMode->setCurrentIndex(input.activePartHvWeightMode==0.0?0:(input.activePartHvWeightMode==1.0?1:-1));
    weightMode->setToolTip(QStringLiteral("仅决定C10、器身重C11及总重C25使用高压裸线重还是绝缘重；材料计价、器身排油和损耗公式不变。"));
    setCellWidget(weightModeRow,4,weightMode);
    connect(weightMode,&QComboBox::currentIndexChanged,this,[this,weightMode,weightModeRow](int index){if(index>=0){item(weightModeRow,4)->setText(QString::number(weightMode->currentData().toInt()));scheduleDesignCore();}});
    addInputRow(row++, QStringLiteral("油顶层温升修正项(K)"), QString::number(input.oilTopCorrection_K),
                QString(), QString(), QStringLiteral("oilTopCorrection"), {});
    addInputRow(row++, "箱底油空(mm)", QString::number(input.tankBottomOil_mm),
                "垫脚高(mm)", QString::number(input.tankFoot_mm),
                "tankBottomOil", "tankFoot");
    addInputRow(row++, "器身侧净空(mm)", QString::number(input.tankSideClear_mm),
                "波纹深(mm)", QString::number(input.waveDepth_mm),
                "tankSideClear", "waveDepth");
    addInputRow(row++, "器身端净空(mm)", QString::number(input.tankEndClear_mm),
                "波纹高(mm)", QString::number(input.waveHeight_mm),
                "tankEndClear", "waveHeight");
    addInputRow(row++, "波纹节距(mm)", QString::number(input.wavePitch_mm),
                "相间距基础(mm)", QString::number(input.phaseGapBase_mm),
                "wavePitch", "phaseGapBase");
}
