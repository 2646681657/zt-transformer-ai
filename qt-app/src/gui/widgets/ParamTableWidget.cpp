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

void ParamTableWidget::loadParams(const TransformerParams &params)
{
    setRowCount(0);

    addSectionRow(0, QStringLiteral("一 输入信息"),
                  QStringLiteral("变压器效率计算方式"), params.efficiencyCalcMethod);
    addParamRow(1, "容量(kVA)", QString::number(params.capacity_kVA), "产品型号", params.productModel);
    addParamRow(2, "高压额定电压(kV)", QString::number(params.hvRatedVoltage_kV), "联结组别", params.connectionGroup);
    addParamRow(3, "低压额定电压(kV)", QString::number(params.lvRatedVoltage_kV), "频率", QString::number(params.frequency_Hz));
    addParamRow(4, "高压调压级数", QString::number(params.hvTapStages), "环境等级", params.environmentGrade);
    addParamRow(5, "高压调压级电压±(%)", QString::number(params.hvTapVoltagePercent), "计算折算温度(℃)", QString::number(params.calcRefTemp_C));
    addParamRow(6, "最高环境温度(℃)", QString::number(params.maxAmbientTemp_C), "铁心截面计算方式", params.coreSectionCalcMethod);
    addParamRow(7, "最高海拔高度(m)", QString::number(params.maxAltitude_m), "负载损耗计算方式", params.loadLossCalcMethod);

    addSectionRow(8, QStringLiteral("二 性能指标"),
                  QStringLiteral("设计允许偏差:最小值(可选填)"), QStringLiteral("最大值(可选填)"));
    addParamRow(9, "空载损耗标准值(W)", QString::number(params.noLoadLossStd_W), "", QString::number(params.noLoadLossMaxDev_pct));
    addParamRow(10, "负载损耗标准值(W)", QString::number(params.loadLossStd_W), "", QString::number(params.loadLossMaxDev_pct));
    addParamRow(11, "总损耗标准值(W)", QString::number(params.totalLossStd_W), "", QString::number(params.totalLossMaxDev_pct));
    addParamRow(12, "阻抗电压标准值(%)", QString::number(params.impedanceVoltageStd_pct),
                QString::number(params.impedanceVoltageMinDev_pct), QString::number(params.impedanceVoltageMaxDev_pct));
    addParamRow(13, "空载电流标准值(%)", QString::number(params.noLoadCurrentStd_pct), "", QString::number(params.noLoadCurrentMaxDev_pct));
    addParamRow(14, "油顶层温升限值(K)", QString::number(params.oilTopTempRise_K));
    addParamRow(15, "高压线圈温升限值(K)", QString::number(params.hvCoilTempRise_K));
    addParamRow(16, "低压线圈温升限值(K)", QString::number(params.lvCoilTempRise_K));
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

// 根据当前结构配置动态生成参数表：不同铁芯/绕组组合显示不同的参数行和分段
void ParamTableWidget::loadParamsForConfig(const TransformerParams &params, const StructureConfig &config)
{
    setRowCount(0);
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

    // 四 铁芯参数（根据结构不同显示不同参数）
    addSectionRow(row++, QStringLiteral("四 铁芯参数"));
    switch (config.coreType) {
    case StructureConfig::PlanarAmorphous:
        addParamRow(row++, "非晶合金带材牌号", "", "叠片系数", "0.86");
        addParamRow(row++, "铁芯柱截面形状", config.coreShape == StructureConfig::EllipseLike ?
                    "类椭圆形" : "矩形", "铁芯重量修正系数", "1.0");
        break;
    case StructureConfig::StackedSilicon:
        addParamRow(row++, "硅钢片牌号", "30QG105", "叠片系数", "0.97");
        addParamRow(row++, "铁芯柱截面形状", config.coreShape == StructureConfig::Circle ?
                    "圆形" : "椭圆形", "铁芯重量修正系数", "1.0");
        addParamRow(row++, "磁通密度(T)", "1.70", "附加损耗系数", "1.0");
        break;
    case StructureConfig::StereoscopicRoll:
        addParamRow(row++, "硅钢片牌号", "23QG090", "叠片系数", "0.97");
        addParamRow(row++, "立体卷截面计算方式", "按叠片", "铁芯重量修正系数", "1.0");
        addParamRow(row++, "磁通密度(T)", "1.65", "附加损耗系数", "1.0");
        break;
    }

    // 五 绕组参数（根据绕组方式和线圈结构不同）
    addSectionRow(row++, QStringLiteral("五 绕组参数"));
    if (config.windingForm == StructureConfig::Dual) {
        addParamRow(row++, "高压绕组匝数范围", "", "低压绕组匝数范围", "");
    } else {
        addParamRow(row++, "高压绕组匝数范围", "", "低压绕组(分裂1)匝数范围", "");
        addParamRow(row++, "", "", "低压绕组(分裂2)匝数范围", "");
    }
    if (config.hvCoilStructure == StructureConfig::MultiLayerCylinder) {
        addParamRow(row++, "高压线圈层数范围", "", "层间绝缘厚度(mm)", "");
    } else {
        addParamRow(row++, "高压线圈段数", "", "段间油道宽度(mm)", "");
    }
    addParamRow(row++, "高压导线截面(mm²)", "", "低压导线/箔材规格", "");
    addParamRow(row++, "线圈高度(mm)", "", "主绝缘距离(mm)", "");
}
