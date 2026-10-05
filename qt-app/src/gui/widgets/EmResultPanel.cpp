#include "EmResultPanel.h"
#include "core/CostBasisNotes.h"
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
    m_oilExpansionTab = createPage(QStringLiteral("油膨缩校核"));
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
         << QStringList{QStringLiteral("宽90补充片叠厚模式"),
                        r.core.yokePiece1Auto ? QStringLiteral("自动（计算单）") : QStringLiteral("手工"), QString()}
         << row(QStringLiteral("实际补充片宽 F19"), r.core.yokePiece1Width_mm, 2, QStringLiteral("mm"))
         << row(QStringLiteral("实际采用叠厚 D16"), r.core.yokePiece1Stack_mm, 3, QStringLiteral("mm"))
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
         << row(QStringLiteral("实际心柱工艺系数"), r.core.coreLossCraftCoef, 6, QString())
         << row(QStringLiteral("实际铁轭工艺系数"), r.core.yokeLossCraftCoef, 6, QString())
         << row(QStringLiteral("心柱铁损分项（合计前未取整）"), r.core.coreLegsLoss_W, 3, QStringLiteral("W"))
         << row(QStringLiteral("铁轭铁损分项（合计前未取整）"), r.core.yokesLoss_W, 3, QStringLiteral("W"))
         << row(QStringLiteral("磁化容量"), r.core.magCapacity_vaPerKg, 3, QStringLiteral("VA/kg"))
         << row(QStringLiteral("硅钢片总重"), r.core.coreWeight_kg, 1, QStringLiteral("kg"))
         << row(QStringLiteral("空载损耗"), r.core.noLoadLoss_W, 1, QStringLiteral("W"))
         << row(QStringLiteral("空载电流"), r.core.noLoadCurrent_pct, 2, QStringLiteral("%"));
    groups.append({ QStringLiteral("铁芯"), core });

    // ---- 绕组 ----
    const auto &w = r.winding;
    QVector<QStringList> winding;
    winding << QStringList{QStringLiteral("油道布局输入提示（非合格判定）"),
                           w.oilDuctLayoutNote.isEmpty() ? QStringLiteral("无单侧启用或端部不连续提示项") : w.oilDuctLayoutNote,
                           QString()};
    winding << QStringList{QStringLiteral("引线损耗记录值（未参与计算）"),
                           QString::number(w.recordedLeadLoss_W, 'g', 15), QStringLiteral("W")}
            << QStringList{QStringLiteral("引线损耗作用"), QStringLiteral("未计入负载损耗、温升或寻优损耗判定"), QString()}
            << QStringList{QStringLiteral("高压试验电压（参考）"), r.testVoltage.highVoltage, QStringLiteral("kV")}
            << QStringList{QStringLiteral("低压试验电压（参考）"), r.testVoltage.lowVoltage, QStringLiteral("kV")}
            << QStringList{QStringLiteral("试验电压依据"), TestVoltageHints::sourceNote(), QString()}
            << QStringList{QStringLiteral("高压参考说明"), r.testVoltage.highReason, QString()}
            << QStringList{QStringLiteral("低压参考说明"), r.testVoltage.lowReason, QString()}
            << row(QStringLiteral("高压匝数（额定）"), double(w.hvTurnsRated), 0, QString())
            << row(QStringLiteral("高压匝数（最大分接）"), double(w.hvTurnsMax), 0, QString())
            << row(QStringLiteral("高压匝数（最小分接）"), double(w.hvTurnsMin), 0, QString())
            << row(QStringLiteral("低压实际匝数（参与计算）"), double(w.lvTurns), 0, QString())
            << row(QStringLiteral("低压参考磁密 AM8（仅推荐用）"), r.core.lvTurnsRecommendation.referenceFlux_T, 3, QStringLiteral("T"))
            << QStringList{QStringLiteral("推荐低压匝数 AN8（未自动采用）"),
                           r.core.lvTurnsRecommendation.error.isEmpty() ? QString::number(r.core.lvTurnsRecommendation.turns)
                               : QStringLiteral("不可用：%1").arg(r.core.lvTurnsRecommendation.error), QString()}
            << row(QStringLiteral("高压每层匝数 Y9"), double(w.layerCount), 0, QStringLiteral("匝/层"))
            << QStringList{QStringLiteral("高压导线形状"), w.hvRoundWire ? QStringLiteral("圆线（计算单表）") : QStringLiteral("扁线"), QString()}
            << QStringList{QStringLiteral("高压导线绝缘种类"),
                           w.hvWireInsulation == QLatin1String("Custom") ? QStringLiteral("自定义") : w.hvWireInsulation,
                           QString()}
            << row(QStringLiteral("高压绝缘总增厚"), w.hvWireInsulAdd_mm, 3, QStringLiteral("mm"))
            << row(QStringLiteral("高压绝缘线宽 X14"), w.hvInsWidth_mm, 3, QStringLiteral("mm"))
            << row(QStringLiteral("高压绝缘线厚 Z14"), w.hvInsThick_mm, 3, QStringLiteral("mm"))
            << row(QStringLiteral("高压单根导线截面 U15"), w.hvWireSection_mm2, 3, QStringLiteral("mm²"))
            << row(QStringLiteral("高压总有效截面 AA15"), w.hvEffectiveSection_mm2, 3, QStringLiteral("mm²"))
            << row(QStringLiteral("低压箔截面"), w.lvWireSection_mm2, 2, QStringLiteral("mm²"))
            << row(QStringLiteral("高压电密 X16"), w.hvCurrentDensity, 3, QStringLiteral("A/mm²"))
            << row(QStringLiteral("低压电密"), w.lvCurrentDensity, 2, QStringLiteral("A/mm²"))
            << row(QStringLiteral("低压辐向厚"), w.lvRadial_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("高压辐向厚"), w.hvRadial_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("主空道"), w.mainDuct_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("高压轴向高"), w.hvAxial_mm, 1, QStringLiteral("mm"))
            << QStringList{QStringLiteral("实际高压线圈型式 AC9"), QString::number(w.hvCoilFormIdx)
                           + (w.hvCoilFormIdx == 1 ? QStringLiteral("（圆筒式）")
                              : QStringLiteral("（暂未开放）")), QString()}
            << row(QStringLiteral("高压段间距 AA29"), w.hvSegmentGap_mm, 2, QStringLiteral("mm"))
            << row(QStringLiteral("高压总轴向高 AA30（含段间距）"), w.hvTotalAxial_mm, 2, QStringLiteral("mm"))
            << row(QStringLiteral("高压内孔轴向高 AC30"), w.hvInnerAxial_mm, 2, QStringLiteral("mm"))
            << row(QStringLiteral("高压端绝缘 AA31"), w.hvEndInsul_mm, 2, QStringLiteral("mm"))
            << row(QStringLiteral("高压端绝缘参考下限 AC32"), w.hvEndInsulReference_mm, 0, QStringLiteral("mm"))
            << QStringList{QStringLiteral("端绝缘参考说明"), QStringLiteral("原计算单参考下限，仅供人工核对；未参与合格判定或寻优筛选"), QString()}
            << row(QStringLiteral("低压轴向高"), w.lvAxial_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("高压平均匝长"), w.hvMeanTurn_m, 4, QStringLiteral("m"))
            << row(QStringLiteral("低压平均匝长"), w.lvMeanTurn_m, 4, QStringLiteral("m"))
            << row(QStringLiteral("高压导线长"), w.hvWireLenRated_m, 2, QStringLiteral("m"))
            << row(QStringLiteral("低压导线长"), w.lvWireLen_m, 2, QStringLiteral("m"))
            << row(QStringLiteral("高压电阻 X19（75℃）"), w.hvResistance_ohm, 6, QStringLiteral("Ω"))
            << row(QStringLiteral("低压电阻（75℃）"), w.lvResistance_ohm, 4, QStringLiteral("Ω"))
            << row(QStringLiteral("高压电阻损耗"), w.hvCopperLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("低压电阻损耗"), w.lvCopperLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("高压附加损耗"), w.hvExtraLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("高压附加损耗率 AA45"), w.hvExtraLossPct, 2, QStringLiteral("%"))
            << QStringList{QStringLiteral("低压附加损耗（采用值）"), QString::number(w.lvExtraLoss_W, 'g', 15), QStringLiteral("W")}
            << QStringList{QStringLiteral("杂散损耗系数 J10"), QString::number(w.strayLossFactor, 'g', 15), QString()}
            << QStringList{QStringLiteral("杂散修正前损耗合计"), QString::number(w.loadLossBeforeStray_W, 'g', 15), QStringLiteral("W")}
            << QStringList{QStringLiteral("低压附加项作用"), w.hvRoundWire
                ? QStringLiteral("圆线按原表L10不计入负载损耗，仍进入低压热负荷；AJ45/AS45口径待核对")
                : QStringLiteral("扁线计入负载损耗及低压热负荷；原表AJ45/AS45口径仍需核对"), QString()}
            << QStringList{QStringLiteral("负载损耗合成规则"), QStringLiteral("杂散修正前合计×(1+杂散系数)，最后取整到整瓦；引线损耗未计入"), QString()}
            << row(QStringLiteral("负载损耗"), w.loadLoss_W, 1, QStringLiteral("W"))
            << row(QStringLiteral("高压裸导线重 W21"), w.hvBareWireWeight_kg, 0, QStringLiteral("kg"))
            << row(QStringLiteral("高压绝缘导线重 Z21"), w.hvWireWeight_kg, 0, QStringLiteral("kg"))
            << row(QStringLiteral("低压导线重"), w.lvWireWeight_kg, 1, QStringLiteral("kg"))
            << row(QStringLiteral("导线总重"), w.wireWeightTotal_kg, 1, QStringLiteral("kg"));
    if (w.hvRoundWire) {
        winding << row(QStringLiteral("圆线裸直径"), w.hvRoundWireDiameter_mm, 3, QStringLiteral("mm"))
                << row(QStringLiteral("圆线表绝缘增重"), w.hvRoundWeightAddPct, 2, QStringLiteral("%"))
                << QStringList{QStringLiteral("圆线附加损耗说明"), QStringLiteral("原表AA45/AC45空值按0；高压热负荷只用电阻损耗"), QString()}
                << QStringList{QStringLiteral("圆线表适用说明"), QStringLiteral("仅计算单缓存表内规格，不插值、不越界；不代表所有材料/绝缘等级"), QString()};
    }
    groups.append({ QStringLiteral("绕组"), winding });

    // ---- 阻抗电压 ----
    const auto &im = r.impedance;
    QVector<QStringList> impedance;
    impedance << row(QStringLiteral("漏磁通道总厚 λ"), im.lambda_mm, 2, QStringLiteral("mm"))
              << row(QStringLiteral("轴向高度差 Q30（含段间距）"), im.axialDifference_mm, 2, QStringLiteral("mm"))
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
            << row(QStringLiteral("高压有效散热高度（AC47高度因子）"), t.hvEffectiveHeight_mm, 2, QStringLiteral("mm"))
            << row(QStringLiteral("波纹散热面积"), t.corrSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("箱顶散热面积"), t.topSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("总散热面积"), t.totalSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("油面温升"), t.oilRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("油顶层温升"), t.oilTopRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("高压绕组温升"), t.hvWindingRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("低压绕组温升 AK52"), t.lvWindingRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("高压散热面积 AC47"), t.hvSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("主空道第二油道 AG43（自动）"), t.mainDuctSecond_mm, 1, QStringLiteral("mm"))
            << row(QStringLiteral("高压等效层间间隙（取整后）"), t.hvLayerGap_mm, 2, QStringLiteral("mm"))
            << row(QStringLiteral("高压表面温升 AC49"), t.hvSurfaceRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("大间隙修正 AC51（≤0.64时为0）"), t.hvGapCorrection_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("层间修正 AC52（负值不计入）"), t.hvLayerCorrection_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("高压对油温升 Y53"), t.hvRiseAboveOil_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("高压热负荷 AB48"), t.hvHeatLoad, 1, QStringLiteral("W/m²"))
            << row(QStringLiteral("低压散热面积 AK47"), t.lvSurface_m2, 2, QStringLiteral("m²"))
            << row(QStringLiteral("低压热负荷 AK48"), t.lvHeatLoad, 2, QStringLiteral("W/m²"))
            << row(QStringLiteral("低压表面温升 AK49"), t.lvSurfaceRise_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("低压层间修正 AK50（负值不计入）"), t.lvLayerCorrection_K, 1, QStringLiteral("K"))
            << row(QStringLiteral("低压对油温升 AK51"), t.lvRiseAboveOil_K, 1, QStringLiteral("K"));
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
         << row(QStringLiteral("材料合计（内置基价）"), c.materialCost, 1, QStringLiteral("元"))
         << QStringList{QStringLiteral("材料成本口径"), CostBasisNotes::engine(), QString()};
    groups.append({ QStringLiteral("重量与成本"), mass });

    const auto &o = r.oilExpansion;
    QVector<QStringList> oil;
    oil << QStringList{QStringLiteral("油膨缩校核（计算单参考）"), o.status(), QString()}
        << QStringList{QStringLiteral("独立校核说明"),
            o.available ? QStringLiteral("N54严格大于N53；与温升分开，不参与寻优筛选") : o.error, QString()}
        << row(QStringLiteral("原表固定参考温差"), o.referenceDeltaT_K, 0, QStringLiteral("K"))
        << row(QStringLiteral("原表膨胀系数"), o.expansionCoefficient, 4, QString());
    if (o.available) {
        oil << row(QStringLiteral("采用总油重 C24"), o.oilWeight_kg, 2, QStringLiteral("kg"))
            << row(QStringLiteral("膨胀需求 N53"), o.demand_kg, 2, QStringLiteral("kg"))
            << row(QStringLiteral("膨缩能力 N54"), o.capacity_kg, 2, QStringLiteral("kg"))
            << row(QStringLiteral("膨缩能力余量"), o.margin_kg, 2, QStringLiteral("kg"))
            << row(QStringLiteral("波纹深 S45"), o.waveDepth_mm, 0, QStringLiteral("mm"))
            << row(QStringLiteral("波纹高 S46"), o.waveHeight_mm, 0, QStringLiteral("mm"))
            << row(QStringLiteral("波纹长边数量 S48"), o.longSideCount, 0, QString())
            << row(QStringLiteral("波纹短边数量 S49"), o.shortSideCount, 0, QString())
            << row(QStringLiteral("波纹系数 Kp S51"), o.kp, 3, QString());
    }
    groups.append({ QStringLiteral("油膨缩校核"), oil });

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
        page->item(i, 1)->setToolTip(rows[i][1]);
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
    fillPage(m_oilExpansionTab, groups[5].second);
}

void EmResultPanel::clearResult()
{
    for (int i = 0; i < count(); ++i) {
        if (auto *page = qobject_cast<QTableWidget *>(widget(i))) {
            page->setRowCount(0);
        }
    }
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
