#include "ParamTableWidget.h"
#include "ModelStdTable.h"
#include <QHeaderView>
#include <QFont>
#include <QComboBox>

ParamTableWidget::ParamTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupTable();
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
    setItem(row, 0, numItem);
    setItem(row, 1, item);
    setItem(row, 2, new QTableWidgetItem(""));
    auto *optNameItem = new QTableWidgetItem(optName);
    optNameItem->setBackground(advanced ? QColor("#4a3210") : QColor("#1a3a4a"));
    setItem(row, 3, optNameItem);
    auto *optValItem = new QTableWidgetItem(optValue);
    optValItem->setBackground(advanced ? QColor("#4a3210") : QColor("#1a3a4a"));
    setItem(row, 4, optValItem);
    setItem(row, 5, new QTableWidgetItem(""));
}

void ParamTableWidget::addParamRow(int row, const QString &name, const QString &value,
                                    const QString &optName, const QString &optValue)
{
    insertRow(row);
    auto *numItem = new QTableWidgetItem(QString::number(row + 1));
    numItem->setTextAlignment(Qt::AlignCenter);
    setItem(row, 0, numItem);
    setItem(row, 1, new QTableWidgetItem(name));
    auto *valItem = new QTableWidgetItem(value);
    setItem(row, 2, valItem);
    setItem(row, 3, new QTableWidgetItem(optName));
    auto *optItem = new QTableWidgetItem(optValue);
    setItem(row, 4, optItem);
    setItem(row, 5, new QTableWidgetItem(""));
}

void ParamTableWidget::bindInput(const QString &key, int row, int col)
{
    if (!key.isEmpty()) {
        m_inputRefs.insert(key, { row, col });
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

    // 输入信息（叠铁芯时容量/型号来自下拉控件，其余来自文本单元格）
    if (m_capacityCombo) {
        params.capacity_kVA = m_capacityCombo->currentData().toDouble();
    } else {
        setDouble("capacity", params.capacity_kVA);
    }
    setDouble("hvRatedVoltage", params.hvRatedVoltage_kV);
    setDouble("lvRatedVoltage", params.lvRatedVoltage_kV);
    setInt("hvTapStages", params.hvTapStages);
    setDouble("hvTapVoltagePercent", params.hvTapVoltagePercent);
    setDouble("maxAmbientTemp", params.maxAmbientTemp_C);
    setDouble("maxAltitude", params.maxAltitude_m);
    setString("efficiencyCalcMethod", params.efficiencyCalcMethod);
    if (m_modelCombo) {
        // 完整型号 = 系列 + 容量（如 SB20-M-630），与铭牌命名一致
        params.productModel = m_modelCombo->currentText() + QStringLiteral("-")
                              + QString::number((int)params.capacity_kVA);
    } else {
        setString("productModel", params.productModel);
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

// 型号/容量联动：查 GB 20052-2024 叠铁芯标准值，覆盖空载/负载/总损耗标准值单元格，
// 并发出摘要信号；联结组别为 Yyn0 时负载损耗取 Yyn0 列，否则取 Dyn11/Yzn11 列。
// 阻抗电压为产品铭牌参数（跟随具体计算单/订单，不随能效等级变化），不参与联动覆盖
void ParamTableWidget::applyModelLinkage()
{
    if (m_loading || !m_modelCombo || !m_capacityCombo) {
        return;
    }
    const QString series = m_modelCombo->currentText();
    const double cap = m_capacityCombo->currentData().toDouble();
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
    // 容差写法（Y,yn0 / Y yn0 等去符号后统一比对）
    const bool yyn0 = cellText(QStringLiteral("connectionGroup"))
                          .remove(QLatin1String(","))
                          .remove(QLatin1String(" "))
                          .contains(QLatin1String("Yyn0"), Qt::CaseInsensitive);
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
    setRowCount(0);
    m_inputRefs.clear();
    int row = 0;

    // 根据变压器结构类型显示不同产品型号前缀
    QString modelPrefix;
    switch (config.coreType) {
    case StructureConfig::StackedSilicon:   modelPrefix = "S"; break;
    case StructureConfig::StereoscopicRoll: modelPrefix = "SZ"; break;
    case StructureConfig::PlanarAmorphous:  modelPrefix = "SBH"; break;
    }

    // 一 输入信息（所有类型共有；静态行同样绑定 key，getParams 按 key 读取）
    addSectionRow(row++, QStringLiteral("一 输入信息"),
                  QStringLiteral("变压器效率计算方式"), params.efficiencyCalcMethod);
    // 容量/产品型号：叠铁芯用下拉（标准 19 档容量 + SB 系列型号），
    // 切换后按 GB 20052-2024 联动覆盖损耗标准值（阻抗为铭牌参数不联动）；
    // 其他铁芯类型保持原文本展示
    if (config.coreType == StructureConfig::StackedSilicon) {
        addParamRow(row, "容量(kVA)", QString::number(params.capacity_kVA),
                    "产品型号", params.productModel);
        m_capacityCombo = new QComboBox(this);
        const QVector<double> caps = ModelStdTable::capacities();
        for (const double c : caps) {
            m_capacityCombo->addItem(QString::number((int)c), c);
        }
        setCellWidget(row, 2, m_capacityCombo);
        m_modelCombo = new QComboBox(this);
        m_modelCombo->addItems({QStringLiteral("SB22-M"), QStringLiteral("SB20-M"),
                                QStringLiteral("SB13-M")});
        setCellWidget(row, 4, m_modelCombo);
        m_loading = true;
        // 非标准容量（如历史方案 750kVA）追加为额外选项，不静默回落到 630；
        // 此时 GB 表查不到标准值，applyModelLinkage 自动跳过（不覆盖任何单元格）
        int ci = m_capacityCombo->findData(params.capacity_kVA);
        if (ci < 0) {
            m_capacityCombo->addItem(QString::number((int)params.capacity_kVA), params.capacity_kVA);
            ci = m_capacityCombo->findData(params.capacity_kVA);
        }
        m_capacityCombo->setCurrentIndex(ci);
        // productModel 可能是完整型号（如 SB20-M-630），取前两段匹配系列
        const int mi = m_modelCombo->findText(params.productModel.section('-', 0, 1));
        m_modelCombo->setCurrentIndex(mi >= 0 ? mi : 1);  // 默认 SB20-M
        m_loading = false;
        connect(m_capacityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { applyModelLinkage(); });
        connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { applyModelLinkage(); });
        ++row;
    } else {
        m_capacityCombo = nullptr;
        m_modelCombo = nullptr;
        addInputRow(row++, "容量(kVA)", QString::number(params.capacity_kVA),
                    "产品型号", modelPrefix + "15-" + QString::number((int)params.capacity_kVA),
                    "capacity", "productModel");
    }
    addInputRow(row++, "高压额定电压(kV)", QString::number(params.hvRatedVoltage_kV),
                "联结组别", params.connectionGroup,
                "hvRatedVoltage", "connectionGroup");
    addParamRow(row++, "低压额定电压(kV)", QString::number(params.lvRatedVoltage_kV),
                "频率", QString::number(params.frequency_Hz));
    bindInput("lvRatedVoltage", row - 1, 2);
    addInputRow(row++, "高压调压级数", QString::number(params.hvTapStages),
                "环境等级", params.environmentGrade,
                "hvTapStages", {});
    addInputRow(row++, "高压调压级电压±(%)", QString::number(params.hvTapVoltagePercent),
                "计算折算温度(℃)", QString::number(params.calcRefTemp_C),
                "hvTapVoltagePercent", {});
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
