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
    setColumnWidth(0, 30);
    setColumnWidth(1, 160);
    setColumnWidth(2, 80);
    setColumnWidth(3, 140);
    setColumnWidth(4, 120);
    verticalHeader()->setVisible(false);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
}

void ParamTableWidget::addSectionRow(int row, const QString &title)
{
    insertRow(row);
    auto *item = new QTableWidgetItem(title);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
    item->setBackground(QColor("#e8f0ff"));
    setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    setItem(row, 1, item);
    setSpan(row, 1, 1, 5);
}

void ParamTableWidget::addParamRow(int row, const QString &name, const QString &value,
                                    const QString &optName, const QString &optValue)
{
    insertRow(row);
    setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
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

    addSectionRow(0, QStringLiteral("一 输入信息"));
    addParamRow(1, "容量(kVA)", QString::number(params.capacity_kVA), "产品型号", params.productModel);
    addParamRow(2, "高压额定电压(kV)", QString::number(params.hvRatedVoltage_kV), "联结组别", params.connectionGroup);
    addParamRow(3, "低压额定电压(kV)", QString::number(params.lvRatedVoltage_kV), "频率", QString::number(params.frequency_Hz));
    addParamRow(4, "高压调压级数", QString::number(params.hvTapStages), "环境等级", params.environmentGrade);
    addParamRow(5, "高压调压级电压±(%)", QString::number(params.hvTapVoltagePercent), "计算折算温度(℃)", QString::number(params.calcRefTemp_C));
    addParamRow(6, "最高环境温度(℃)", QString::number(params.maxAmbientTemp_C), "铁心截面计算方式", params.coreSectionCalcMethod);
    addParamRow(7, "最高海拔高度(m)", QString::number(params.maxAltitude_m), "负载损耗计算方式", params.loadLossCalcMethod);

    addSectionRow(8, QStringLiteral("二 性能指标"));
    addParamRow(9, "空载损耗标准值(W)", QString::number(params.noLoadLossStd_W), "", "");
    addParamRow(10, "空载损耗最大允许偏差(%)", QString::number(params.noLoadLossMaxDev_pct), "", "");
    addParamRow(11, "负载损耗标准值(W)", QString::number(params.loadLossStd_W), "", "");
    addParamRow(12, "负载损耗最大允许偏差(%)", QString::number(params.loadLossMaxDev_pct), "", "");
    addParamRow(13, "总损耗标准值(W)", QString::number(params.totalLossStd_W), "", "");
    addParamRow(14, "总损耗最大允许偏差(%)", QString::number(params.totalLossMaxDev_pct), "", "");
    addParamRow(15, "阻抗电压标准值(%)", QString::number(params.impedanceVoltageStd_pct), "", "");
    addParamRow(16, "阻抗电压最大允许偏差(%)", QString::number(params.impedanceVoltageMaxDev_pct), "", "");
    addParamRow(17, "阻抗电压最小允许偏差(%)", QString::number(params.impedanceVoltageMinDev_pct), "", "");
    addParamRow(18, "空载电流标准值(%)", QString::number(params.noLoadCurrentStd_pct), "", "");
    addParamRow(19, "空载电流最大允许偏差(%)", QString::number(params.noLoadCurrentMaxDev_pct), "", "");
    addParamRow(20, "油顶层温升限值(K)", QString::number(params.oilTopTempRise_K), "", "");
    addParamRow(21, "高压线圈温升限值(K)", QString::number(params.hvCoilTempRise_K), "", "");
    addParamRow(22, "低压线圈温升限值(K)", QString::number(params.lvCoilTempRise_K), "", "");
}

TransformerParams ParamTableWidget::getParams() const
{
    TransformerParams params;
    // Row 1 = capacity
    if (item(1, 2)) params.capacity_kVA = item(1, 2)->text().toDouble();
    if (item(2, 2)) params.hvRatedVoltage_kV = item(2, 2)->text().toDouble();
    if (item(3, 2)) params.lvRatedVoltage_kV = item(3, 2)->text().toDouble();
    if (item(4, 2)) params.hvTapStages = item(4, 2)->text().toInt();
    if (item(5, 2)) params.hvTapVoltagePercent = item(5, 2)->text().toDouble();
    if (item(6, 2)) params.maxAmbientTemp_C = item(6, 2)->text().toDouble();
    if (item(7, 2)) params.maxAltitude_m = item(7, 2)->text().toDouble();
    if (item(9, 2)) params.noLoadLossStd_W = item(9, 2)->text().toDouble();
    if (item(11, 2)) params.loadLossStd_W = item(11, 2)->text().toDouble();
    if (item(15, 2)) params.impedanceVoltageStd_pct = item(15, 2)->text().toDouble();
    return params;
}
