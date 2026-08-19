#include "ParamTableWidget.h"
#include <QHeaderView>
#include <QFont>

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
                                      const QString &optName, const QString &optValue)
{
    insertRow(row);
    auto *item = new QTableWidgetItem(title);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
    item->setBackground(QColor("#1a3a4a"));
    auto *numItem = new QTableWidgetItem(QString::number(row + 1));
    numItem->setTextAlignment(Qt::AlignCenter);
    setItem(row, 0, numItem);
    setItem(row, 1, item);
    setItem(row, 2, new QTableWidgetItem(""));
    auto *optNameItem = new QTableWidgetItem(optName);
    optNameItem->setBackground(QColor("#1a3a4a"));
    setItem(row, 3, optNameItem);
    auto *optValItem = new QTableWidgetItem(optValue);
    optValItem->setBackground(QColor("#1a3a4a"));
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
    if (item(1, 2)) params.capacity_kVA = item(1, 2)->text().toDouble();
    if (item(2, 2)) params.hvRatedVoltage_kV = item(2, 2)->text().toDouble();
    if (item(3, 2)) params.lvRatedVoltage_kV = item(3, 2)->text().toDouble();
    if (item(4, 2)) params.hvTapStages = item(4, 2)->text().toInt();
    if (item(5, 2)) params.hvTapVoltagePercent = item(5, 2)->text().toDouble();
    if (item(6, 2)) params.maxAmbientTemp_C = item(6, 2)->text().toDouble();
    if (item(7, 2)) params.maxAltitude_m = item(7, 2)->text().toDouble();
    if (item(0, 4)) params.efficiencyCalcMethod = item(0, 4)->text();
    if (item(1, 4)) params.productModel = item(1, 4)->text();
    if (item(9, 2)) params.noLoadLossStd_W = item(9, 2)->text().toDouble();
    if (item(10, 2)) params.loadLossStd_W = item(10, 2)->text().toDouble();
    if (item(12, 2)) params.impedanceVoltageStd_pct = item(12, 2)->text().toDouble();
    return params;
}

// 根据当前结构配置动态生成参数表：不同铁芯/绕组组合显示不同的参数行和分段；
// 四/五/六节为可编辑设计变量，与 CalcInput 双向同步（saveToInput 读回）
void ParamTableWidget::loadParamsForConfig(const TransformerParams &params, const StructureConfig &config,
                                           const CalcInput &input)
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

    // 一 输入信息（所有类型共有）
    addSectionRow(row++, QStringLiteral("一 输入信息"),
                  QStringLiteral("变压器效率计算方式"), params.efficiencyCalcMethod);
    addParamRow(row++, "容量(kVA)", QString::number(params.capacity_kVA),
                "产品型号", modelPrefix + "15-" + QString::number((int)params.capacity_kVA));
    addParamRow(row++, "高压额定电压(kV)", QString::number(params.hvRatedVoltage_kV),
                "联结组别", params.connectionGroup);
    addParamRow(row++, "低压额定电压(kV)", QString::number(params.lvRatedVoltage_kV),
                "频率", QString::number(params.frequency_Hz));
    addParamRow(row++, "高压调压级数", QString::number(params.hvTapStages),
                "环境等级", params.environmentGrade);
    addParamRow(row++, "高压调压级电压±(%)", QString::number(params.hvTapVoltagePercent),
                "计算折算温度(℃)", QString::number(params.calcRefTemp_C));
    addParamRow(row++, "最高环境温度(℃)", QString::number(params.maxAmbientTemp_C),
                "铁心截面计算方式", params.coreSectionCalcMethod);
    addParamRow(row++, "最高海拔高度(m)", QString::number(params.maxAltitude_m),
                "负载损耗计算方式", params.loadLossCalcMethod);

    // 二 性能指标
    addSectionRow(row++, QStringLiteral("二 性能指标"),
                  QStringLiteral("设计允许偏差:最小值(可选填)"), QStringLiteral("最大值(可选填)"));
    addParamRow(row++, "空载损耗标准值(W)", QString::number(params.noLoadLossStd_W),
                "", QString::number(params.noLoadLossMaxDev_pct));
    addParamRow(row++, "负载损耗标准值(W)", QString::number(params.loadLossStd_W),
                "", QString::number(params.loadLossMaxDev_pct));
    addParamRow(row++, "总损耗标准值(W)", QString::number(params.totalLossStd_W),
                "", QString::number(params.totalLossMaxDev_pct));
    addParamRow(row++, "阻抗电压标准值(%)", QString::number(params.impedanceVoltageStd_pct),
                QString::number(params.impedanceVoltageMinDev_pct),
                QString::number(params.impedanceVoltageMaxDev_pct));
    addParamRow(row++, "空载电流标准值(%)", QString::number(params.noLoadCurrentStd_pct),
                "", QString::number(params.noLoadCurrentMaxDev_pct));

    // 三 温升限值（根据铁芯类型不同）
    addSectionRow(row++, QStringLiteral("三 温升限值"));
    if (config.coreType == StructureConfig::PlanarAmorphous) {
        addParamRow(row++, "油顶层温升限值(K)", QString::number(params.oilTopTempRise_K));
    }
    addParamRow(row++, "高压线圈温升限值(K)", QString::number(params.hvCoilTempRise_K));
    addParamRow(row++, "低压线圈温升限值(K)", QString::number(params.lvCoilTempRise_K));

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
}
