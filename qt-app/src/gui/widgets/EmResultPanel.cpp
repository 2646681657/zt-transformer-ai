#include "EmResultPanel.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QVector>
#include <QStringList>

namespace {

// 数值格式化行 {参数名, 数值, 单位}
QStringList row(const QString &name, double value, int prec, const QString &unit)
{
    return { name, QString::number(value, 'f', prec), unit };
}

} // namespace

EmResultPanel::EmResultPanel(QWidget *parent)
    : QTabWidget(parent)
{
    m_coreTab = createPage(QStringLiteral("铁芯"));
    m_windingTab = createPage(QStringLiteral("绕组"));
    m_impedanceTab = createPage(QStringLiteral("阻抗电压"));
    m_thermalTab = createPage(QStringLiteral("温升"));
    m_massTab = createPage(QStringLiteral("重量与成本"));
}

QTableWidget *EmResultPanel::createPage(const QString &title)
{
    auto *page = new QTableWidget(this);
    page->setColumnCount(3);
    page->setHorizontalHeaderLabels({ QStringLiteral("参数名称"),
                                      QStringLiteral("数值"),
                                      QStringLiteral("单位") });
    page->verticalHeader()->setVisible(false);
    page->setAlternatingRowColors(true);
    page->setEditTriggers(QAbstractItemView::NoEditTriggers);
    page->setSelectionBehavior(QAbstractItemView::SelectRows);
    page->horizontalHeader()->setStretchLastSection(true);
    page->setColumnWidth(0, 220);
    page->setColumnWidth(1, 120);
    addTab(page, title);
    return page;
}

QVector<QPair<QString, QVector<QStringList>>> EmResultPanel::buildGroups(const CalcResult &r)
{
    QVector<QPair<QString, QVector<QStringList>>> groups;

    // ---- 铁芯 ----
    QVector<QStringList> core;
    core << row(QStringLiteral("匝电压"), r.core.turnVoltage_V, 4, QStringLiteral("V"))
         << row(QStringLiteral("大圆半径"), r.core.majorRadius_mm, 2, QStringLiteral("mm"))
         << row(QStringLiteral("圆心到轴距离"), r.core.yokeFlat_mm, 2, QStringLiteral("mm"))
         << row(QStringLiteral("交接点高"), r.core.junctionHeight_mm, 2, QStringLiteral("mm"))
         << row(QStringLiteral("短轴长"), r.core.minorAxis_mm, 2, QStringLiteral("mm"))
         << row(QStringLiteral("心柱截面"), r.core.coreArea_cm2, 2, QStringLiteral("cm²"))
         << row(QStringLiteral("铁轭截面"), r.core.yokeArea_cm2, 2, QStringLiteral("cm²"))
         << row(QStringLiteral("心柱磁密"), r.core.fluxDensity_core_T, 3, QStringLiteral("T"))
         << row(QStringLiteral("铁轭磁密"), r.core.fluxDensity_yoke_T, 3, QStringLiteral("T"))
         << row(QStringLiteral("心柱单位铁损"), r.core.coreLossPerKg_W, 3, QStringLiteral("W/kg"))
         << row(QStringLiteral("铁轭单位铁损"), r.core.yokeLossPerKg_W, 3, QStringLiteral("W/kg"))
         << row(QStringLiteral("磁化容量"), r.core.magCapacity_vaPerKg, 3, QStringLiteral("VA/kg"))
         << row(QStringLiteral("硅钢片总重"), r.core.coreWeight_kg, 1, QStringLiteral("kg"))
         << row(QStringLiteral("空载损耗"), r.core.noLoadLoss_W, 1, QStringLiteral("W"))
         << row(QStringLiteral("空载电流"), r.core.noLoadCurrent_pct, 2, QStringLiteral("%"));
    groups.append({ QStringLiteral("铁芯"), core });

    // ---- 绕组 ----
    const auto &w = r.winding;
    QVector<QStringList> winding;
    winding << row(QStringLiteral("高压匝数（额定）"), double(w.hvTurnsRated), 0, QString())
            << row(QStringLiteral("高压匝数（最大分接）"), double(w.hvTurnsMax), 0, QString())
            << row(QStringLiteral("高压匝数（最小分接）"), double(w.hvTurnsMin), 0, QString())
            << row(QStringLiteral("低压匝数"), double(w.lvTurns), 0, QString())
            << row(QStringLiteral("每段层数"), double(w.layerCount), 0, QString())
            << row(QStringLiteral("高压导线截面"), w.hvWireSection_mm2, 3, QStringLiteral("mm²"))
            << row(QStringLiteral("低压箔截面"), w.lvWireSection_mm2, 2, QStringLiteral("mm²"))
            << row(QStringLiteral("高压电密"), w.hvCurrentDensity, 2, QStringLiteral("A/mm²"))
            << row(QStringLiteral("低压电密"), w.lvCurrentDensity, 2, QStringLiteral("A/mm²"))
            << row(QStringLiteral("低压辐向厚"), w.lvRadial_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("高压辐向厚"), w.hvRadial_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("主空道"), w.mainDuct_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("高压轴向高"), w.hvAxial_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("低压轴向高"), w.lvAxial_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("高压平均匝长"), w.hvMeanTurn_m, 4, QStringLiteral("m"))
            << row(QStringLiteral("低压平均匝长"), w.lvMeanTurn_m, 4, QStringLiteral("m"))
            << row(QStringLiteral("高压导线长"), w.hvWireLenRated_m, 2, QStringLiteral("m"))
            << row(QStringLiteral("低压导线长"), w.lvWireLen_m, 2, QStringLiteral("m"))
            << row(QStringLiteral("高压电阻（75℃）"), w.hvResistance_ohm, 4, QStringLiteral("Ω"))
            << row(QStringLiteral("低压电阻（75℃）"), w.lvResistance_ohm, 4, QStringLiteral("Ω"))
            << row(QStringLiteral("高压电阻损耗"), w.hvCopperLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("低压电阻损耗"), w.lvCopperLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("高压附加损耗"), w.hvExtraLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("负载损耗"), w.loadLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("高压导线重"), w.hvWireWeight_kg, 1, QStringLiteral("kg"))
            << row(QStringLiteral("低压导线重"), w.lvWireWeight_kg, 1, QStringLiteral("kg"))
            << row(QStringLiteral("导线总重"), w.wireWeightTotal_kg, 1, QStringLiteral("kg"));
    groups.append({ QStringLiteral("绕组"), winding });

    // ---- 阻抗电压 ----
    const auto &im = r.impedance;
    QVector<QStringList> impedance;
    impedance << row(QStringLiteral("漏磁通道总厚 λ"), im.lambda_mm, 2, QStringLiteral("mm"))
              << row(QStringLiteral("绕组电抗高"), im.hx_mm, 1, QStringLiteral("mm"))
              << row(QStringLiteral("低压漏磁折算厚"), im.a2, 2, QStringLiteral("mm"))
              << row(QStringLiteral("高压漏磁折算厚"), im.a1, 2, QStringLiteral("mm"))
              << row(QStringLiteral("漏磁面积"), im.leakArea_mm2, 2, QStringLiteral("mm²"))
              << row(QStringLiteral("横向漏磁系数"), im.kx, 2, QString())
              << row(QStringLiteral("电阻压降"), im.resistanceDrop_pct, 2, QStringLiteral("%"))
              << row(QStringLiteral("电抗压降"), im.reactanceDrop_pct, 2, QStringLiteral("%"))
              << row(QStringLiteral("阻抗电压"), im.impedance_pct, 2, QStringLiteral("%"));
    groups.append({ QStringLiteral("阻抗电压"), impedance });

    // ---- 温升 ----
    const auto &t = r.thermal;
    QVector<QStringList> thermal;
    thermal << row(QStringLiteral("箱壁散热面积"), t.tankSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("波纹散热面积"), t.corrSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("箱顶散热面积"), t.topSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("总散热面积"), t.totalSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("油面温升"), t.oilRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("油顶层温升"), t.oilTopRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("高压绕组温升"), t.hvWindingRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("低压绕组温升"), t.lvWindingRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("高压热负荷"), t.hvHeatLoad, 1, QString())
            << row(QStringLiteral("低压热负荷"), t.lvHeatLoad, 1, QString());
    groups.append({ QStringLiteral("温升"), thermal });

    // ---- 重量与成本 ----
    const auto &m = r.mass;
    const auto &c = r.cost;
    QVector<QStringList> mass;
    mass << row(QStringLiteral("窗高"), m.windowHeight_mm, 1, QStringLiteral("mm"))
         << row(QStringLiteral("中心距"), m.centerDistance_mm, 1, QStringLiteral("mm"))
         << row(QStringLiteral("器身重"), m.activePartWeight_kg, 1, QStringLiteral("kg"))
         << row(QStringLiteral("油箱长"), m.tankLength_mm, 1, QStringLiteral("mm"))
         << row(QStringLiteral("油箱宽"), m.tankWidth_mm, 1, QStringLiteral("mm"))
         << row(QStringLiteral("油箱高"), m.tankHeight_mm, 1, QStringLiteral("mm"))
         << row(QStringLiteral("油箱及附件重"), m.tankWeight_kg, 1, QStringLiteral("kg"))
         << row(QStringLiteral("总油重"), m.oilWeight_kg, 1, QStringLiteral("kg"))
         << row(QStringLiteral("变压器总重"), m.totalWeight_kg, 1, QStringLiteral("kg"))
         << row(QStringLiteral("硅钢片成本"), c.steelCost, 1, QStringLiteral("元"))
         << row(QStringLiteral("高压导线成本"), c.hvWireCost, 1, QStringLiteral("元"))
         << row(QStringLiteral("低压箔成本"), c.lvWireCost, 1, QStringLiteral("元"))
         << row(QStringLiteral("绝缘油成本"), c.oilCost, 1, QStringLiteral("元"))
         << row(QStringLiteral("油箱成本"), c.tankCost, 1, QStringLiteral("元"))
         << row(QStringLiteral("材料成本合计"), c.materialCost, 1, QStringLiteral("元"));
    groups.append({ QStringLiteral("重量与成本"), mass });

    return groups;
}

void EmResultPanel::fillPage(QTableWidget *page, const QVector<QStringList> &rows)
{
    page->setRowCount(0);
    for (int i = 0; i < rows.size(); ++i) {
        page->insertRow(i);
        page->setItem(i, 0, new QTableWidgetItem(rows[i][0]));
        page->setItem(i, 1, new QTableWidgetItem(rows[i][1]));
        page->setItem(i, 2, new QTableWidgetItem(rows[i].value(2)));
    }
}

void EmResultPanel::loadResult(const CalcResult &result)
{
    const auto groups = buildGroups(result);
    fillPage(m_coreTab, groups[0].second);
    fillPage(m_windingTab, groups[1].second);
    fillPage(m_impedanceTab, groups[2].second);
    fillPage(m_thermalTab, groups[3].second);
    fillPage(m_massTab, groups[4].second);
}

QString EmResultPanel::resultText(const CalcResult &result)
{
    QString text;
    const auto groups = buildGroups(result);
    for (const auto &group : groups) {
        text += QStringLiteral("======== %1 ========\n").arg(group.first);
        for (const auto &r : group.second) {
            text += QStringLiteral("%1: %2 %3\n")
                        .arg(r[0], r[1], r.value(2));
        }
        text += QStringLiteral("\n");
    }
    return text.trimmed();
}
