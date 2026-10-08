#include "ParamTableWidget.h"
#include "ModelStdTable.h"
#include "DesignDatabase.h"
#include "CalculationApplicability.h"
#include "ElectromagneticEngine.h"
#include "TestVoltageHints.h"
#include "HvCoilFormNotes.h"
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
#include <cmath>

namespace {
enum class SupportedConnection { Invalid, Dyn11, Yyn0 };

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
    return SupportedConnection::Invalid;
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
        for (const QString &key : {QStringLiteral("connectionGroup"), QStringLiteral("frequency"),
                                  QStringLiteral("noLoadLossStd"), QStringLiteral("loadLossStd"),
                                  QStringLiteral("totalLossStd")}) {
            const auto it = m_inputRefs.constFind(key);
            if (it == m_inputRefs.constEnd() || changed->row() != it->first || changed->column() != it->second)
                continue;
            if (key.endsWith(QStringLiteral("LossStd"))) {
                m_lossStandardsManual = true;
                m_lastLinkageKey = standardKey();
                m_standardMode = TransformerParams::StandardMode::Custom;
                if (m_standardModeCombo) {
                    const QSignalBlocker blocker(m_standardModeCombo);
                    m_standardModeCombo->setCurrentIndex(1);
                }
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
    }
    params.capacity_kVA = input.capacity_kVA;
    params.hvRatedVoltage_kV = input.hvRated_kV;
    params.lvRatedVoltage_kV = input.lvRated_kV;
    params.hvTapPlusSteps = input.hvTapPlusSteps;
    params.hvTapMinusSteps = input.hvTapMinusSteps;
    params.hvTapVoltagePercent = input.hvTapStep_pct;
    params.connectionGroup = input.lvStarConnected
        ? (input.hvDeltaConnected ? QStringLiteral("Dyn11") : QStringLiteral("Yyn0"))
        : (input.hvDeltaConnected ? QStringLiteral("Dd（暂不支持）") : QStringLiteral("Yd（暂不支持）"));
    static const QRegularExpression seriesPattern(QStringLiteral("^(.+-M)(?:-|$)"),
                                                  QRegularExpression::CaseInsensitiveOption);
    const auto match = seriesPattern.match(params.productModel.trimmed());
    const QString series = match.hasMatch() ? match.captured(1) : QStringLiteral("SB20-M");
    params.productModel = QStringLiteral("%1-%2/%3-%4")
        .arg(series, QString::number(input.capacity_kVA, 'g', 12),
             QString::number(input.hvRated_kV, 'g', 12), QString::number(input.lvRated_kV, 'g', 12));
    return params;
}

bool ParamTableWidget::collectForCalculation(TransformerParams &params, CalcInput &input, QString &error,
                                            bool interactive)
{
    error.clear();
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
    if (!hasSupportedConnectionGroup()) {
        error = QStringLiteral("当前版本仅支持 Dyn11（可填 Dyn）和 Yyn0，请修改联结组别后再计算。");
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
    const QStringList textKeys = {"connectionGroup", "environmentGrade", "efficiencyCalcMethod"};
    const QStringList integerKeys = {"frequency", "seamCount", "lvTurns", "hvTurnsPerLayer", "hvParallelCount",
        "hvStackCount", "lvLayerInsulCount", "yokeWidenStages", "waveDepth", "waveHeight", "wavePitch"};
    for (auto it = m_inputRefs.constBegin(); it != m_inputRefs.constEnd(); ++it) {
        if (textKeys.contains(it.key()))
            continue;
        const auto *cell = item(it->first, it->second);
        bool ok = false;
        const QString value = cell ? cell->text().trimmed() : QString();
        const double number = value.toDouble(&ok);
        if (ok && integerKeys.contains(it.key()))
            value.toInt(&ok);
        if (!ok || !std::isfinite(number)) {
            const auto *label = item(it->first, it->second == 2 ? 1 : 3);
            error = QStringLiteral("“%1”请输入有效%2，不能留空或输入非数字。")
                .arg(label && !label->text().isEmpty() ? label->text() : it.key(),
                     integerKeys.contains(it.key()) ? QStringLiteral("整数") : QStringLiteral("数值"));
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
        error = scopeError + QStringLiteral("。请使用油浸式、低压箔绕、叠铁芯、椭圆形、双绕组、多层圆筒式，50Hz/75℃配置。");
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
    const auto it = m_inputRefs.constFind(QStringLiteral("connectionGroup"));
    return it != m_inputRefs.constEnd() && item(it->first, it->second) &&
           connectionType(item(it->first, it->second)->text()) != SupportedConnection::Invalid;
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
    if (!hasSupportedConnectionGroup())
        return QStringLiteral("当前联结组别无对应的内置损耗标准");
    return QString();
}

void ParamTableWidget::applyModelLinkage()
{
    updateTestVoltageHints();
    if (m_loading || m_modelSeries.isEmpty() || m_modelCapacity_kVA <= 0.0) {
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
    if (m_standardMode == TransformerParams::StandardMode::Unconfirmed) {
        m_standardStatus = QStringLiteral("指标来源待确认：旧方案或指标数据缺失；保留当前值，请选择模式并逐项核对，不能视为该方案原指标。");
    } else if (m_standardMode == TransformerParams::StandardMode::Custom) {
        m_standardStatus = QStringLiteral("非标：使用手工性能指标；更改型号、容量和接法不自动覆盖指标，不代表已满足合同要求。");
    } else if (!unavailable.isEmpty()) {
        m_standardStatus = unavailable + QStringLiteral("；需手工核对：保留原损耗值，未自动匹配当前规格。");
    } else {
        if (key != m_lastLinkageKey)
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
        m_standardStatus = QStringLiteral("%1/%2kVA：%3；阻抗、空载电流标准保留手工值。")
            .arg(series).arg(cap)
            .arg(m_lossStandardsManual ? QStringLiteral("使用手工损耗标准") : QStringLiteral("已匹配内置10kV损耗表"));
    }
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
            ? QStringLiteral("仅损耗表自动；其余指标手工") : m_standardMode == TransformerParams::StandardMode::Custom
            ? QStringLiteral("全部指标手工；规格切换不覆盖") : QStringLiteral("旧方案来源缺失，请核对"));
        item(m_standardModeRow, 5)->setToolTip(m_standardStatus);
    }
    emit stdValuesUpdated(m_standardStatus);
    updateYokePiece1();
    updateLvTurnsRecommendation();
    updateDesignConnection();
    scheduleDesignCore();
}

void ParamTableWidget::scheduleDesignCore()
{
    if (m_loading || !m_designConnectionUi || m_designCoreRow < 0) return;
    showDesignCore(nullptr, QStringLiteral("正在更新当前输入；旧预览已失效"));
    m_corePreviewTimer->start();
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
    const bool supported = hasSupportedConnectionGroup();
    const auto type = connectionType(item(m_designConnectionRow, 2)->text());
    const QString note = supported
        ? (type == SupportedConnection::Dyn11 ? QStringLiteral("高压D / 低压yn；内置匹配时按Dyn列")
                                             : QStringLiteral("高压Y / 低压yn；内置匹配时按Yyn列"))
        : QStringLiteral("当前接法尚未支持；禁止计算，不沿用旧接法");
    item(m_designConnectionRow, 5)->setText(note + QStringLiteral("；仅50Hz"));
    item(m_designConnectionRow, 5)->setToolTip(item(m_designConnectionRow, 5)->text());
    if (!supported) error = QStringLiteral("当前联结组别未支持，请选择Dyn11或Yyn0");
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
            cell->setToolTip(error.isEmpty() ? QStringLiteral("当前已提交额定值与接法，复用引擎基础电量公式；只读，不改实际匝数。") : error);
        }
        auto *remark = item(m_designPhaseRow + r, 5);
        remark->setText(error.isEmpty() ? QStringLiteral("仅基础电量联动；不代表完整方案可算或合格") : error);
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
void ParamTableWidget::loadParamsForConfig(const TransformerParams &params, const StructureConfig &config,
                                           const CalcInput &input, bool proMode, bool designConnectionUi)
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
    m_config = config;
    m_lossStandardsManual = params.lossStandardsManual;
    m_standardMode = params.standardMode;
    if (m_standardMode == TransformerParams::StandardMode::BuiltIn && m_lossStandardsManual)
        m_standardMode = TransformerParams::StandardMode::Custom;
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
    if (m_designConnectionUi) {
        m_designConnectionRow = row - 1;
        m_designConnectionCombo = new QComboBox(this);
        m_designConnectionCombo->addItem(QStringLiteral("Dyn（Dyn11）"), QStringLiteral("Dyn11"));
        m_designConnectionCombo->addItem(QStringLiteral("Yyn（Yyn0）"), QStringLiteral("Yyn0"));
        m_designConnectionCombo->addItem(QStringLiteral("Dd（暂不支持计算）"), QStringLiteral("Dd"));
        m_designConnectionCombo->addItem(QStringLiteral("Yd（暂不支持计算）"), QStringLiteral("Yd"));
        QString selected = params.connectionGroup.trimmed();
        const auto type = connectionType(selected);
        if (type == SupportedConnection::Dyn11) selected = QStringLiteral("Dyn11");
        else if (type == SupportedConnection::Yyn0) selected = QStringLiteral("Yyn0");
        int selectedIndex = m_designConnectionCombo->findData(selected);
        if (selectedIndex < 0) {
            m_designConnectionCombo->addItem(selected + QStringLiteral("（未支持，需修改）"), selected);
            selectedIndex = m_designConnectionCombo->count() - 1;
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
    if (m_designConnectionUi) {
        m_standardModeRow = row;
        addParamRow(row++, QStringLiteral("指标来源模式"), QString(), QString(), QString());
        item(m_standardModeRow, 2)->setFlags(item(m_standardModeRow, 2)->flags() & ~Qt::ItemIsEditable);
        m_standardModeCombo = new QComboBox(this);
        auto *modeCombo = m_standardModeCombo;
        modeCombo->addItem(QStringLiteral("内置标准（损耗表）"), int(TransformerParams::StandardMode::BuiltIn));
        modeCombo->addItem(QStringLiteral("非标（手工指标）"), int(TransformerParams::StandardMode::Custom));
        modeCombo->addItem(QStringLiteral("来源待确认"), int(TransformerParams::StandardMode::Unconfirmed));
        modeCombo->setCurrentIndex(int(m_standardMode));
        setCellWidget(m_standardModeRow, 2, modeCombo);
        connect(modeCombo, &QComboBox::currentIndexChanged, this, [this, modeCombo](int index) {
            if (m_loading || modeCombo != m_standardModeCombo) return;
            const auto previous = m_standardMode;
            const auto next = TransformerParams::StandardMode(modeCombo->itemData(index).toInt());
            if (next == TransformerParams::StandardMode::BuiltIn) {
                const QString unavailable = standardUnavailableReason();
                if (!unavailable.isEmpty() || QMessageBox::question(this, QStringLiteral("切回内置损耗标准"),
                    QStringLiteral("将按当前型号、容量和接法替换空载、负载及总损耗指标；其余指标和允许偏差保持手工值。是否继续？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                    if (!unavailable.isEmpty()) QMessageBox::warning(this, QStringLiteral("内置表不适用"), unavailable);
                    const QSignalBlocker blocker(modeCombo);
                    modeCombo->setCurrentIndex(int(previous));
                    return;
                }
            }
            m_standardMode = next;
            m_lossStandardsManual = (next != TransformerParams::StandardMode::BuiltIn);
            m_lastLinkageKey = standardKey();
            applyModelLinkage();
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
    addInputRow(row++, "接缝数", QString::number(input.seamCount), {}, {}, "seamCount", {});

    addInputRow(row++, "宽90补充片叠厚模式", QString(),
                "实际叠厚(mm)", QString::number(input.yokePiece1Stack_mm, 'g', 15), {}, "yokePiece1Stack");
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
    addInputRow(row++, "主空道宽(mm)", QString::number(input.mainDuctWidth_mm),
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
            ? QStringLiteral("分段：两段串联、两段算一层；暂不开放计算")
            : QStringLiteral("W12总层数；Y9自动计算；分段工艺口径见悬停提示"));
    }
}

// 从表格设计变量节读回 CalcInput：空值/非法值保持原字段不变
void ParamTableWidget::saveToInput(CalcInput &input) const
{
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
            input.hvDeltaConnected = (type == SupportedConnection::Dyn11);
            input.lvStarConnected = true;
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
    for (int i = 0; i < 16; ++i) {
        setDouble(QStringLiteral("yokeStep_%1").arg(i), input.yokeSteps_mm[i]);
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
    setDouble("tankBottomOil", input.tankBottomOil_mm);
    setDouble("tankSideClear", input.tankSideClear_mm);
    setDouble("tankEndClear", input.tankEndClear_mm);
    setDouble("tankFoot", input.tankFoot_mm);
    setInt("waveDepth", input.waveDepth_mm);
    setInt("waveHeight", input.waveHeight_mm);
    setInt("wavePitch", input.wavePitch_mm);
    setDouble("phaseGapBase", input.phaseGapBase_mm);
}

// 专业模式追加的高级参数节（七~十）：铁芯工艺 / 绕组油道 / 损耗系数 / 油箱结构，
// 数组类参数（轭阶梯 16 级、轴向油道各 5 位）平铺为逐行展示
void ParamTableWidget::addProModeSections(int &row, const CalcInput &input)
{
    // 七 铁芯工艺
    addSectionRow(row++, QStringLiteral("七 铁芯工艺（高级）"), {}, {}, true);
    addInputRow(row++, "轭片放大片宽(mm)", QString::number(input.yokeWidenTo_mm),
                "", "", "yokeWidenTo", {});
    addInputRow(row++, "T形轭片叠厚-宽80(mm)", QString::number(input.yokePiece2Stack_mm),
                "轭片放大级数", QString::number(input.yokeWidenStages),
                "yokePiece2Stack", "yokeWidenStages");
    addInputRow(row++, "T形轭片叠厚-宽60(mm)", QString::number(input.yokePiece3Stack_mm),
                "", "",
                "yokePiece3Stack", {});
    for (int i = 0; i < 16; ++i) {
        addInputRow(row++, QStringLiteral("轭阶梯 %1 外伸半宽(mm)").arg(i + 1),
                    QString::number(input.yokeSteps_mm[i]),
                    (i == 0 ? QStringLiteral("(-1=自动按(C5-Ci)/2)") : QString()),
                    QString(),
                    QStringLiteral("yokeStep_%1").arg(i), {});
    }

    // 八 绕组工艺（油道与绝缘细节）
    addSectionRow(row++, QStringLiteral("八 绕组工艺（高级）"), {}, {}, true);
    addInputRow(row++, QStringLiteral("油道宽/高含义"), QStringLiteral("相间侧/端部侧"),
                QStringLiteral("各侧停用值"), QStringLiteral("0"), {}, {});
    for (int col : {2, 4})
        item(row - 1, col)->setFlags(item(row - 1, col)->flags() & ~Qt::ItemIsEditable);
    item(row - 1, 5)->setText(QStringLiteral("两侧独立，尺寸非负；异常布局需核对"));
    item(row - 1, 5)->setToolTip(QStringLiteral("原表按相间侧和端部侧分别判0；可单侧停用，不自动配对、重排或改变公式。单侧启用或端部不连续布局仅提示需人工核对。"));
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
    const QString lvExtraLossNote = QStringLiteral("扁线计入负载损耗及低压热负荷AK48；圆线按计算单L10不计入负载损耗，但仍进入低压热负荷。不同于仅记录的引线损耗；原表AJ45/AS45口径仍待核对。");
    item(row - 1, 1)->setToolTip(lvExtraLossNote);
    item(row - 1, 2)->setToolTip(lvExtraLossNote);
    item(row - 1, 5)->setText(QStringLiteral("扁线计入损耗及温升；圆线仅低压温升；原表口径待核对"));
    item(row - 1, 5)->setToolTip(lvExtraLossNote);

    // 十 油箱与结构
    addSectionRow(row++, QStringLiteral("十 油箱与结构（高级）"), {}, {}, true);
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
