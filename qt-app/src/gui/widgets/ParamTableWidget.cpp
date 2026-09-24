#include "ParamTableWidget.h"
#include "ModelStdTable.h"
#include <QHeaderView>
#include <QFont>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

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
    connect(this, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *changed) {
        const auto it = m_inputRefs.constFind(QStringLiteral("connectionGroup"));
        if (!m_loading && it != m_inputRefs.constEnd() &&
            changed->row() == it->first && changed->column() == it->second) {
            applyModelLinkage();
        }
    });
}

void ParamTableWidget::setupTable()
{
    setColumnCount(6);
    setHorizontalHeaderLabels({"#", "参数名称", "数值", "选项名称", "选项", "备注"});
    horizontalHeader()->setStretchLastSection(true);
    setColumnWidth(0, 40);
    setColumnWidth(1, 160);
    setColumnWidth(2, 80);
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
    item->setBackground(advanced ? QColor("#4a3210") : QColor("#1a3a4a"));
    item->setForeground(advanced ? QColor("#ffb74d") : QColor("#e0e6ed"));
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
    optNameItem->setBackground(advanced ? QColor("#4a3210") : QColor("#1a3a4a"));
    setItem(row, 3, optNameItem);
    auto *optValItem = new QTableWidgetItem(optValue);
    optValItem->setFlags(optValItem->flags() & ~Qt::ItemIsEditable);
    optValItem->setBackground(advanced ? QColor("#4a3210") : QColor("#1a3a4a"));
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
    TransformerParams params;
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

bool ParamTableWidget::hasSupportedConnectionGroup() const
{
    const auto it = m_inputRefs.constFind(QStringLiteral("connectionGroup"));
    return it != m_inputRefs.constEnd() && item(it->first, it->second) &&
           connectionType(item(it->first, it->second)->text()) != SupportedConnection::Invalid;
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
    token.replace(QLatin1Char(','), QLatin1Char('.'));
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
    token.replace(QLatin1Char(','), QLatin1Char('.'));
    const double hv = token.toDouble(&ok);
    if (ok && hv > 0.0) {
        m_modelHvRated_kV = hv;
    }

    if (!commitLowVoltage) {
        return true;
    }
    token = value.mid(voltageSep + 1).trimmed();
    token.replace(QLatin1Char(','), QLatin1Char('.'));
    const double lv = token.toDouble(&ok);
    if (ok && lv > 0.0) {
        m_modelLvRated_kV = lv;
    }
    return true;
}

void ParamTableWidget::applyModelLinkage()
{
    if (m_loading || m_modelSeries.isEmpty() || m_modelCapacity_kVA <= 0.0) {
        return;
    }
    const QString series = m_modelSeries;
    const double cap = m_modelCapacity_kVA;
    const ModelStdTable::StdEntry *e = ModelStdTable::lookup(series, cap);
    if (!e) {
        return;
    }
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
    // 当前标准表只包含 Dyn11 和 Yyn0；其他接线不能套用 Dyn 的标准损耗。
    const SupportedConnection connection = connectionType(cellText(QStringLiteral("connectionGroup")));
    if (connection == SupportedConnection::Invalid) {
        emit stdValuesUpdated(QStringLiteral("当前联结组别暂无对应标准损耗；首版支持 Dyn11、Yyn0"));
        return;
    }
    const bool yyn0 = connection == SupportedConnection::Yyn0;
    const double loadW = yyn0 ? e->loadYyn0_W : e->loadDyn_W;
    setCell(QStringLiteral("noLoadLossStd"), QString::number(e->noLoad_W));
    setCell(QStringLiteral("loadLossStd"), QString::number(loadW));
    setCell(QStringLiteral("totalLossStd"), QString::number(e->noLoad_W + loadW));
    emit stdValuesUpdated(QStringLiteral("已联动 %1/%2kVA：空载 %3W、负载 %4W、总损耗 %5W（阻抗标准值保持当前设置）")
                              .arg(series).arg((int)cap)
                              .arg(e->noLoad_W).arg(loadW)
                              .arg(e->noLoad_W + loadW));
}

// 根据当前结构配置动态生成参数表：不同铁芯/绕组组合显示不同的参数行和分段；
// 四/五/六节为可编辑设计变量，与 CalcInput 双向同步（saveToInput 读回）；
// proMode=true 时追加七~十节高级参数（专业模式）
void ParamTableWidget::loadParamsForConfig(const TransformerParams &params, const StructureConfig &config,
                                           const CalcInput &input, bool proMode)
{
    m_tapPlusSpin = nullptr;
    m_tapMinusSpin = nullptr;
    m_tapStepSpin = nullptr;
    m_hvMaterialCombo = nullptr;
    m_lvMaterialCombo = nullptr;
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
        QStringLiteral("^(.+-M)(?:-[0-9]+(?:[.,][0-9]+)?){0,2}$"),
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
             QString::number(params.lvRatedVoltage_kV, 'g', 12).replace(QLatin1Char('.'), QLatin1Char(',')));

    addParamRow(row, QStringLiteral("产品型号"), compositeModel);
    item(row, 5)->setText(QStringLiteral("型号-M-容量/高压-低压"));
    m_productModelEdit = new QLineEdit(this);
    m_productModelEdit->setText(compositeModel);
    m_productModelEdit->setPlaceholderText(QStringLiteral("例如：SB20-M-630/10-0,4"));
    m_productModelEdit->setToolTip(QStringLiteral(
        "输入“/”后保存容量并联动标准值；输入高压后的“-”保存高压；输入低压后按回车保存"));
    setCellWidget(row, 2, m_productModelEdit);
    setSpan(row, 2, 1, 3);
    m_modelSeries = series;
    m_modelCapacity_kVA = params.capacity_kVA;
    m_modelHvRated_kV = params.hvRatedVoltage_kV;
    m_modelLvRated_kV = params.lvRatedVoltage_kV;
    m_lastLinkageKey.clear();
    connect(m_productModelEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        const bool capacityComplete = parseCompositeModel(text, false);
        const QString linkageKey = m_modelSeries + QLatin1Char('/')
                                 + QString::number(m_modelCapacity_kVA, 'g', 12);
        if (capacityComplete && linkageKey != m_lastLinkageKey) {
            m_lastLinkageKey = linkageKey;
            applyModelLinkage();
        }
    });
    connect(m_productModelEdit, &QLineEdit::returnPressed, this, [this]() {
        parseCompositeModel(m_productModelEdit->text(), true);
    });
    ++row;

    addInputRow(row++, "联结组别", params.connectionGroup,
                "频率(Hz)", QString::number(params.frequency_Hz),
                "connectionGroup", {});
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
    addInputRow(row++, "最高环境温度(℃)", QString::number(params.maxAmbientTemp_C),
                "铁心截面计算方式", params.coreSectionCalcMethod,
                "maxAmbientTemp", {});
    addInputRow(row++, "最高海拔高度(m)", QString::number(params.maxAltitude_m),
                "负载损耗计算方式", params.loadLossCalcMethod,
                "maxAltitude", {});
    bindInput("efficiencyCalcMethod", 0, 4);

    // 二 性能指标
    addSectionRow(row++, QStringLiteral("二 性能指标"),
                  QStringLiteral("设计允许偏差:最小值(可选填)"), QStringLiteral("最大值(可选填)"));
    addInputRow(row++, "空载损耗标准值(W)", QString::number(params.noLoadLossStd_W),
                "", QString::number(params.noLoadLossMaxDev_pct),
                "noLoadLossStd", "noLoadLossMaxDev");
    addInputRow(row++, "负载损耗标准值(W)", QString::number(params.loadLossStd_W),
                "", QString::number(params.loadLossMaxDev_pct),
                "loadLossStd", "loadLossMaxDev");
    addInputRow(row++, "总损耗标准值(W)", QString::number(params.totalLossStd_W),
                "", QString::number(params.totalLossMaxDev_pct),
                "totalLossStd", "totalLossMaxDev");
    addInputRow(row++, "阻抗电压标准值(%)", QString::number(params.impedanceVoltageStd_pct),
                QString::number(params.impedanceVoltageMinDev_pct),
                QString::number(params.impedanceVoltageMaxDev_pct),
                "impedanceVoltageStd", "impedanceVoltageMaxDev");
    bindInput("impedanceVoltageMinDev", row - 1, 3);
    addInputRow(row++, "空载电流标准值(%)", QString::number(params.noLoadCurrentStd_pct),
                "", QString::number(params.noLoadCurrentMaxDev_pct),
                "noLoadCurrentStd", "noLoadCurrentMaxDev");

    // 三 温升限值（根据铁芯类型不同）
    addSectionRow(row++, QStringLiteral("三 温升限值"));
    if (config.coreType == StructureConfig::PlanarAmorphous) {
        addParamRow(row++, "油顶层温升限值(K)", QString::number(params.oilTopTempRise_K));
        bindInput("oilTopTempRise", row - 1, 2);
    }
    addParamRow(row++, "高压线圈温升限值(K)", QString::number(params.hvCoilTempRise_K));
    bindInput("hvCoilTempRise", row - 1, 2);
    addParamRow(row++, "低压线圈温升限值(K)", QString::number(params.lvCoilTempRise_K));
    bindInput("lvCoilTempRise", row - 1, 2);

    // 四 铁芯参数（设计变量，初值取自 CalcInput）
    addSectionRow(row++, QStringLiteral("四 铁芯参数"));
    addInputRow(row++, "铁芯直径(mm)", QString::number(input.coreDiameter_mm),
                "叠片系数", QString::number(input.stackFactor),
                "coreDiameter", "stackFactor");
    addInputRow(row++, "直线段长(mm)", QString::number(input.coreStraight_mm),
                "椭圆角(°)", QString::number(input.ellipseAngle_deg),
                "coreStraight", "ellipseAngle");
    addInputRow(row++, "硅钢片牌号", input.steelGrade,
                "硅钢片厚(mm)", QString::number(input.steelThickness_mm),
                "steelGrade", "steelThickness");
    addInputRow(row++, "铁损工艺系数", QString::number(input.coreLossCraftCoef),
                "接缝数", QString::number(input.seamCount),
                "coreLossCraftCoef", "seamCount");

    // 五 绕组参数（设计变量，初值取自 CalcInput）
    addSectionRow(row++, QStringLiteral("五 绕组参数"));
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
                "低压箔厚(mm)", QString::number(input.lvFoilThick_mm),
                "lvTurns", "lvFoilThick");
    addInputRow(row++, "低压箔宽(mm)", QString::number(input.lvFoilWidth_mm),
                "低压端绝缘(mm)", QString::number(input.lvEndInsul_mm),
                "lvFoilWidth", "lvEndInsul");
    addInputRow(row++, "高压裸线宽(mm)", QString::number(input.hvBareWidth_mm),
                "高压裸线厚(mm)", QString::number(input.hvBareThick_mm),
                "hvBareWidth", "hvBareThick");
    addInputRow(row++, "高压每层匝数", QString::number(input.hvTurnsPerLayer),
                "层间绝缘厚(mm)", QString::number(input.hvLayerInsul_mm),
                "hvTurnsPerLayer", "hvLayerInsul");
    addInputRow(row++, "高压并绕根数", QString::number(input.hvParallelCount),
                "高压叠绕根数", QString::number(input.hvStackCount),
                "hvParallelCount", "hvStackCount");

    // 六 主空道（设计变量，初值取自 CalcInput）
    addSectionRow(row++, QStringLiteral("六 主空道"));
    addInputRow(row++, "主空道宽(mm)", QString::number(input.mainDuctWidth_mm),
                "纸板厚(mm)", QString::number(input.mainDuctInsul_mm),
                "mainDuctWidth", "mainDuctInsul");

    // 七~十 高级参数（仅专业模式）
    if (proMode) {
        addProModeSections(row, input);
    }
}

// 从表格设计变量节读回 CalcInput：空值/非法值保持原字段不变
void ParamTableWidget::saveToInput(CalcInput &input) const
{
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
    setDouble("steelThickness", input.steelThickness_mm);
    setDouble("coreLossCraftCoef", input.coreLossCraftCoef);
    setInt("seamCount", input.seamCount);
    const QString grade = cellText("steelGrade");
    if (!grade.isEmpty()) {
        input.steelGrade = grade;
    }

    // 低压绕组
    setInt("lvTurns", input.lvTurns);
    setDouble("lvFoilThick", input.lvFoilThick_mm);
    setDouble("lvFoilWidth", input.lvFoilWidth_mm);
    setDouble("lvEndInsul", input.lvEndInsul_mm);

    // 高压绕组
    setDouble("hvBareWidth", input.hvBareWidth_mm);
    setDouble("hvBareThick", input.hvBareThick_mm);
    setInt("hvTurnsPerLayer", input.hvTurnsPerLayer);
    setDouble("hvLayerInsul", input.hvLayerInsul_mm);
    setInt("hvParallelCount", input.hvParallelCount);
    setInt("hvStackCount", input.hvStackCount);

    // 主空道
    setDouble("mainDuctWidth", input.mainDuctWidth_mm);
    setDouble("mainDuctInsul", input.mainDuctInsul_mm);

    // ---- 以下为专业模式高级参数（未绑定时保持原值）----

    // 七 铁芯工艺
    setDouble("yokePiece1Stack", input.yokePiece1Stack_mm);
    setDouble("yokePiece2Stack", input.yokePiece2Stack_mm);
    setDouble("yokePiece3Stack", input.yokePiece3Stack_mm);
    setDouble("yokeWidenTo", input.yokeWidenTo_mm);
    setInt("yokeWidenStages", input.yokeWidenStages);
    for (int i = 0; i < 16; ++i) {
        setDouble(QStringLiteral("yokeStep_%1").arg(i), input.yokeSteps_mm[i]);
    }

    // 八 绕组工艺（油道与绝缘细节）
    setDouble("hvWireInsulAdd", input.hvWireInsulAdd_mm);
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
    setDouble("refFluxDens", input.refFluxDens_T);
}

// 专业模式追加的高级参数节（七~十）：铁芯工艺 / 绕组油道 / 损耗系数 / 油箱结构，
// 数组类参数（轭阶梯 16 级、轴向油道各 5 位）平铺为逐行展示
void ParamTableWidget::addProModeSections(int &row, const CalcInput &input)
{
    // 七 铁芯工艺
    addSectionRow(row++, QStringLiteral("七 铁芯工艺（高级）"), {}, {}, true);
    addInputRow(row++, "T形轭片叠厚-宽90(mm)", QString::number(input.yokePiece1Stack_mm),
                "轭片放大片宽(mm)", QString::number(input.yokeWidenTo_mm),
                "yokePiece1Stack", "yokeWidenTo");
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
    addInputRow(row++, "高压导线绝缘增厚(mm)", QString::number(input.hvWireInsulAdd_mm),
                "低压层间绝缘层数", QString::number(input.lvLayerInsulCount),
                "hvWireInsulAdd", "lvLayerInsulCount");
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
                "引线损耗(W)", QString::number(input.leadLoss_W),
                "strayLossFactor", "leadLoss");
    addInputRow(row++, "低压附加损耗(W)", QString::number(input.lvExtraLoss_W),
                "", "",
                "lvExtraLoss", {});

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
    addInputRow(row++, "低压参考磁密(T)", QString::number(input.refFluxDens_T),
                "", "",
                "refFluxDens", {});
}
