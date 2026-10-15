#include "SchemeCalculationSheet.h"
#include "ParamTableWidget.h"
#include "EmResultPanel.h"
#include "CalculationSheetTemplate.h"
#include "CalculationSheetDescriptions.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QEvent>
#include <QStyledItemDelegate>
#include <QPersistentModelIndex>
#include <QToolButton>
#include <algorithm>

namespace {
enum Zone { Basic, Performance, Thermal, Core, High, Low, Duct, Tank, Reference, ZoneCount };
struct Field {
    QString name, value, tip;
    int row = -1, col = -1;
    bool wide = false;
    QString key;
};
// 原生单元格也在输入时提交给绑定源；同值回刷不重置光标/撤销历史。
class LiveInputDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        auto *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto *line = qobject_cast<QLineEdit *>(editor)) {
            const QPersistentModelIndex persistent(index);
            connect(line, &QLineEdit::textEdited, line, [persistent](const QString &text) {
                if (persistent.isValid()) const_cast<QAbstractItemModel *>(persistent.model())->setData(persistent, text, Qt::EditRole);
            });
        }
        return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto *line = qobject_cast<QLineEdit *>(editor);
        if (line && line->hasFocus() && line->text() == index.data(Qt::EditRole).toString()) return;
        QStyledItemDelegate::setEditorData(editor, index);
    }
};

// Semantic result key -> owning 计算单 address. Different worksheets are never aliased.
const QHash<QString, QString> &originalResultAddresses()
{
    static const QHash<QString, QString> addresses = {
        {QStringLiteral("绕组\n高压导线形状"), QStringLiteral("AC13")},
        {QStringLiteral("绕组\n实际高压线圈型式 AC9"), QStringLiteral("AC9")},
        {QStringLiteral("铁芯\n匝电压"), QStringLiteral("AC4")},
        {QStringLiteral("铁芯\n大圆半径"), QStringLiteral("M20")},
        {QStringLiteral("铁芯\n圆心到轴距离"), QStringLiteral("O20")},
        {QStringLiteral("铁芯\n交接点高"), QStringLiteral("R20")},
        {QStringLiteral("铁芯\n短轴长"), QStringLiteral("L20")},
        {QStringLiteral("铁芯\n心柱截面"), QStringLiteral("F14")},
        {QStringLiteral("铁芯\n心柱磁密"), QStringLiteral("F16")},
        {QStringLiteral("铁芯\n磁化容量"), QStringLiteral("J16")},
        {QStringLiteral("铁芯\n硅钢片总重"), QStringLiteral("C9")},
        {QStringLiteral("铁芯\n空载损耗"), QStringLiteral("O16")},
        {QStringLiteral("铁芯\n空载电流"), QStringLiteral("R16")},
        {QStringLiteral("绕组\n高压匝数（额定）"), QStringLiteral("Y8")},
        {QStringLiteral("绕组\n高压匝数（最大分接）"), QStringLiteral("V8")},
        {QStringLiteral("绕组\n高压匝数（最小分接）"), QStringLiteral("AA8")},
        {QStringLiteral("绕组\n高压每层匝数 Y9"), QStringLiteral("Y9")},
        {QStringLiteral("绕组\n高压绝缘线宽 X14"), QStringLiteral("X14")},
        {QStringLiteral("绕组\n高压绝缘线厚 Z14"), QStringLiteral("Z14")},
        {QStringLiteral("绕组\n高压单根导线截面 U15"), QStringLiteral("U15")},
        {QStringLiteral("绕组\n高压总有效截面 AA15"), QStringLiteral("AA15")},
        {QStringLiteral("绕组\n低压箔截面"), QStringLiteral("AH15")},
        {QStringLiteral("绕组\n高压电密 X16"), QStringLiteral("X16")},
        {QStringLiteral("绕组\n低压电密"), QStringLiteral("AH16")},
        {QStringLiteral("绕组\n低压辐向厚"), QStringLiteral("AK23")},
        {QStringLiteral("绕组\n高压辐向厚"), QStringLiteral("W28")},
        {QStringLiteral("绕组\n主空道"), QStringLiteral("AK43")},
        {QStringLiteral("绕组\n高压轴向高"), QStringLiteral("AA28")},
        {QStringLiteral("绕组\n高压段间距 AA29"), QStringLiteral("AA29")},
        {QStringLiteral("绕组\n高压总轴向高 AA30（含段间距）"), QStringLiteral("AA30")},
        {QStringLiteral("绕组\n高压内孔轴向高 AC30"), QStringLiteral("AC30")},
        {QStringLiteral("绕组\n高压端绝缘 AA31"), QStringLiteral("AA31")},
        {QStringLiteral("绕组\n高压端绝缘参考下限 AC32"), QStringLiteral("AC32")},
        {QStringLiteral("绕组\n低压轴向高"), QStringLiteral("AA33")},
        {QStringLiteral("绕组\n高压平均匝长"), QStringLiteral("X17")},
        {QStringLiteral("绕组\n低压平均匝长"), QStringLiteral("AH17")},
        {QStringLiteral("绕组\n高压导线长"), QStringLiteral("Z18")},
        {QStringLiteral("绕组\n低压导线长"), QStringLiteral("AH18")},
        {QStringLiteral("绕组\n高压电阻 X19（75℃）"), QStringLiteral("X19")},
        {QStringLiteral("绕组\n低压电阻（75℃）"), QStringLiteral("AH19")},
        {QStringLiteral("绕组\n高压电阻损耗"), QStringLiteral("Y20")},
        {QStringLiteral("绕组\n低压电阻损耗"), QStringLiteral("AH20")},
        {QStringLiteral("绕组\n高压附加损耗"), QStringLiteral("AC45")},
        {QStringLiteral("绕组\n高压附加损耗率 AA45"), QStringLiteral("AA45")},
        {QStringLiteral("绕组\n负载损耗"), QStringLiteral("L10")},
        {QStringLiteral("绕组\n高压裸导线重 W21"), QStringLiteral("W21")},
        {QStringLiteral("绕组\n高压绝缘导线重 Z21"), QStringLiteral("Z21")},
        {QStringLiteral("绕组\n低压导线重"), QStringLiteral("AH21")},
        {QStringLiteral("绕组\n导线总重"), QStringLiteral("C10")},
        {QStringLiteral("阻抗电压\n漏磁通道总厚 λ"), QStringLiteral("M39")},
        {QStringLiteral("阻抗电压\n轴向高度差 Q30（含段间距）"), QStringLiteral("Q30")},
        {QStringLiteral("阻抗电压\n绕组电抗高"), QStringLiteral("Q33")},
        {QStringLiteral("阻抗电压\n低压漏磁折算厚"), QStringLiteral("R40")},
        {QStringLiteral("阻抗电压\n高压漏磁折算厚"), QStringLiteral("R41")},
        {QStringLiteral("阻抗电压\n漏磁面积"), QStringLiteral("O42")},
        {QStringLiteral("阻抗电压\n横向漏磁系数"), QStringLiteral("Q32")},
        {QStringLiteral("阻抗电压\n电阻压降"), QStringLiteral("Q34")},
        {QStringLiteral("阻抗电压\n电抗压降"), QStringLiteral("P43")},
        {QStringLiteral("阻抗电压\n阻抗电压"), QStringLiteral("Q35")},
        {QStringLiteral("温升\n箱壁散热面积"), QStringLiteral("O44")},
        {QStringLiteral("温升\n波纹散热面积"), QStringLiteral("O45")},
        {QStringLiteral("温升\n箱顶散热面积"), QStringLiteral("O47")},
        {QStringLiteral("温升\n总散热面积"), QStringLiteral("O48")},
        {QStringLiteral("温升\n油面温升"), QStringLiteral("N49")},
        {QStringLiteral("温升\n油顶层温升"), QStringLiteral("N51")},
        {QStringLiteral("温升\n高压绕组温升"), QStringLiteral("Y54")},
        {QStringLiteral("温升\n低压绕组温升 AK52"), QStringLiteral("AK52")},
        {QStringLiteral("温升\n高压散热面积 AC47"), QStringLiteral("AC47")},
        {QStringLiteral("温升\n主空道第二油道 AG43（自动）"), QStringLiteral("AG43")},
        {QStringLiteral("温升\n高压表面温升 AC49"), QStringLiteral("AC49")},
        {QStringLiteral("温升\n大间隙修正 AC51（≤0.64时为0）"), QStringLiteral("AC51")},
        {QStringLiteral("温升\n层间修正 AC52（负值不计入）"), QStringLiteral("AC52")},
        {QStringLiteral("温升\n高压对油温升 Y53"), QStringLiteral("Y53")},
        {QStringLiteral("温升\n高压热负荷 AB48"), QStringLiteral("AB48")},
        {QStringLiteral("温升\n低压散热面积 AK47"), QStringLiteral("AK47")},
        {QStringLiteral("温升\n低压热负荷 AK48"), QStringLiteral("AK48")},
        {QStringLiteral("温升\n低压表面温升 AK49"), QStringLiteral("AK49")},
        {QStringLiteral("温升\n低压层间修正 AK50（负值不计入）"), QStringLiteral("AK50")},
        {QStringLiteral("温升\n低压对油温升 AK51"), QStringLiteral("AK51")},
        {QStringLiteral("重量与成本\n窗高"), QStringLiteral("J14")},
        {QStringLiteral("重量与成本\n中心距"), QStringLiteral("L14")},
        {QStringLiteral("重量与成本\n器身重"), QStringLiteral("C11")},
        {QStringLiteral("重量与成本\n油箱长"), QStringLiteral("H26")},
        {QStringLiteral("重量与成本\n油箱宽"), QStringLiteral("F25")},
        {QStringLiteral("重量与成本\n油箱高"), QStringLiteral("J27")},
        {QStringLiteral("重量与成本\n油箱及附件重"), QStringLiteral("C19")},
        {QStringLiteral("重量与成本\n总油重"), QStringLiteral("C24")},
        {QStringLiteral("重量与成本\n变压器总重"), QStringLiteral("C25")},
        {QStringLiteral("绕组\n低压实际匝数（参与计算）"), QStringLiteral("AH8")},
        {QStringLiteral("绕组\n高压导线绝缘种类"), QStringLiteral("U14")},
        {QStringLiteral("绕组\n杂散损耗系数 J10"), QStringLiteral("J10")},
        {QStringLiteral("绕组\n高压试验电压（参考）"), QStringLiteral("C7")},
        {QStringLiteral("绕组\n低压试验电压（参考）"), QStringLiteral("C8")},
        {QStringLiteral("铁芯\n实际补充片宽 F19"), QStringLiteral("F19")},
        {QStringLiteral("铁芯\n实际采用叠厚 D16"), QStringLiteral("F20")},
        {QStringLiteral("铁芯\n小圆心角 P12（原表名称）"), QStringLiteral("P12")},
        {QStringLiteral("铁芯\n大圆心角 S12（原表名称）"), QStringLiteral("S12")},
        {QStringLiteral("油膨缩校核\n采用总油重 C24"), QStringLiteral("C24")},
        {QStringLiteral("油膨缩校核\n膨胀需求 N53"), QStringLiteral("N53")},
        {QStringLiteral("油膨缩校核\n膨缩能力 N54"), QStringLiteral("N54")},
        {QStringLiteral("油膨缩校核\n波纹深 S45"), QStringLiteral("S45")},
        {QStringLiteral("油膨缩校核\n波纹高 S46"), QStringLiteral("S46")},
        {QStringLiteral("油膨缩校核\n波纹长边数量 S48"), QStringLiteral("S48")},
        {QStringLiteral("油膨缩校核\n波纹短边数量 S49"), QStringLiteral("S49")},
        {QStringLiteral("油膨缩校核\n波纹系数 Kp S51"), QStringLiteral("S51")},
        {QStringLiteral("基础电量\n高压额定线电压"), QStringLiteral("Y5")},
        {QStringLiteral("基础电量\n高压最高分接线电压"), QStringLiteral("V5")},
        {QStringLiteral("基础电量\n高压最低分接线电压"), QStringLiteral("AA5")},
        {QStringLiteral("基础电量\n高压额定相电压"), QStringLiteral("Y6")},
        {QStringLiteral("基础电量\n高压最高分接相电压"), QStringLiteral("V6")},
        {QStringLiteral("基础电量\n高压最低分接相电压"), QStringLiteral("AA6")},
        {QStringLiteral("基础电量\n高压线电流"), QStringLiteral("W7")},
        {QStringLiteral("基础电量\n高压相电流"), QStringLiteral("Z7")},
        {QStringLiteral("基础电量\n低压线电压"), QStringLiteral("AH5")},
        {QStringLiteral("基础电量\n低压相电压"), QStringLiteral("AH6")},
        {QStringLiteral("基础电量\n低压相电流"), QStringLiteral("AG7")},
    };
    return addresses;
}

auto sheetGroups(const CalcResult &result)
{
    auto groups = EmResultPanel::buildGroups(result);
    const auto snapshotValue = [&result](double value, int precision) {
        return result.valid && result.linkageSnapshotAvailable
            ? QString::number(value, 'f', precision) : QStringLiteral("待重新计算");
    };
    QVector<QStringList> electrical;
    const auto &e = result.electrical;
    electrical << QStringList{QStringLiteral("高压额定线电压"), snapshotValue(e.hvLineRated_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("高压最高分接线电压"), snapshotValue(e.hvLineMax_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("高压最低分接线电压"), snapshotValue(e.hvLineMin_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("高压额定相电压"), snapshotValue(e.hvPhaseRated_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("高压最高分接相电压"), snapshotValue(e.hvPhaseMax_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("高压最低分接相电压"), snapshotValue(e.hvPhaseMin_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("高压线电流"), snapshotValue(e.hvLineCurrent_A, 2), QStringLiteral("A")}
               << QStringList{QStringLiteral("高压相电流"), snapshotValue(e.hvPhaseCurrent_A, 2), QStringLiteral("A")}
               << QStringList{QStringLiteral("低压线电压"), snapshotValue(e.lvLine_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("低压相电压"), snapshotValue(e.lvPhase_V, 0), QStringLiteral("V")}
               << QStringList{QStringLiteral("低压相电流"), snapshotValue(e.lvPhaseCurrent_A, 2), QStringLiteral("A")};
    groups.append({QStringLiteral("基础电量"), electrical});

    auto coreGroup = std::find_if(groups.begin(), groups.end(), [](const auto &group) {
        return group.first == QStringLiteral("铁芯");
    });
    if (coreGroup != groups.end()) {
        auto &core = coreGroup->second;
        const auto &c = result.core;
        core << QStringList{QStringLiteral("小圆心角 P12（原表名称）"), snapshotValue(c.majorAngle_deg, 0), QStringLiteral("°")}
             << QStringList{QStringLiteral("大圆心角 S12（原表名称）"), snapshotValue(c.minorAngle_deg, 0), QStringLiteral("°")}
             << QStringList{QStringLiteral("心柱实际截面 I24"), snapshotValue(c.coreAreaActual_cm2, 2), QStringLiteral("cm²")}
             << QStringList{QStringLiteral("心柱实际查表磁密 I25"), snapshotValue(c.fluxDensity_coreActual_T, 3), QStringLiteral("T")}
             << QStringList{QStringLiteral("铁轭实际截面 O24"), snapshotValue(c.yokeAreaActual_cm2, 2), QStringLiteral("cm²")}
             << QStringList{QStringLiteral("心柱分项重量 P23"), snapshotValue(c.coreLegsWeight_kg, 1), QStringLiteral("kg")}
             << QStringList{QStringLiteral("铁轭分项重量 U23"), snapshotValue(c.yokesWeight_kg, 1), QStringLiteral("kg")};
        // 固定展示全部16级；片宽为几何快照，叠厚不是片数取整后的实际叠厚。
        for (int i = 0; i < 16; ++i) {
            const bool available = result.valid && result.linkageSnapshotAvailable
                && i < c.widths_mm.size() && i < c.stacks_mm.size();
            const QString value = available
                ? QStringLiteral("%1 / %2")
                    .arg(QString::number(c.widths_mm[i], 'f', 1), QString::number(c.stacks_mm[i], 'f', 1))
                : QStringLiteral("待重新计算");
            core.append({QStringLiteral("第%1级片宽 / 几何叠厚").arg(i + 1), value, QStringLiteral("mm")});
        }
    }
    // Exact workbook addresses are local to the 计算单 worksheet (not Sheet1).
    QVector<QStringList> original;
    const auto at = [&](const char *addresses, double value, int precision) {
        for (const auto &address : QString::fromLatin1(addresses).split(QLatin1Char(' ')))
            original.append({address, result.sheetLayoutSnapshotAvailable ? snapshotValue(value, precision) : QStringLiteral("待重新计算"), QString()});
    };
    at("AC4", result.core.turnVoltage_V, 4);
    at("C9 N14", result.core.coreWeight_kg, 1);
    at("F14", result.core.coreArea_cm2, 2);
    at("F16", result.core.fluxDensity_core_T, 3);
    at("J16", result.core.magCapacity_vaPerKg, 3);
    at("O16 M10", result.core.noLoadLoss_W, 1);
    at("R16 O10", result.core.noLoadCurrent_pct, 2);
    at("P12", result.core.majorAngle_deg, 0);
    at("S12", result.core.minorAngle_deg, 0);
    at("L20", result.core.minorAxis_mm, 2);
    at("M20", result.core.majorRadius_mm, 2);
    at("O20", result.core.yokeFlat_mm, 2);
    at("R20", result.core.junctionHeight_mm, 2);
    at("V5", result.electrical.hvLineMax_V, 0);
    at("Y5", result.electrical.hvLineRated_V, 0);
    at("AA5", result.electrical.hvLineMin_V, 0);
    at("V6", result.electrical.hvPhaseMax_V, 0);
    at("Y6", result.electrical.hvPhaseRated_V, 0);
    at("AA6", result.electrical.hvPhaseMin_V, 0);
    at("W7", result.electrical.hvLineCurrent_A, 2);
    at("Z7", result.electrical.hvPhaseCurrent_A, 2);
    at("AH5", result.electrical.lvLine_V, 0);
    at("AH6", result.electrical.lvPhase_V, 0);
    at("AG7", result.electrical.lvPhaseCurrent_A, 2);
    at("V8 AA12", result.winding.hvTurnsMax, 0);
    at("Y8", result.winding.hvTurnsRated, 0);
    at("AA8", result.winding.hvTurnsMin, 0);
    at("Y9 Y10", result.winding.layerCount, 0);
    at("AC9", result.winding.hvCoilFormIdx, 0);
    at("X14 W22", result.winding.hvInsWidth_mm, 3);
    at("Z14", result.winding.hvInsThick_mm, 3);
    at("U15", result.winding.hvWireSection_mm2, 3);
    at("AA15", result.winding.hvEffectiveSection_mm2, 3);
    at("AH15", result.winding.lvWireSection_mm2, 3);
    at("X16", result.winding.hvCurrentDensity, 3);
    at("AH16", result.winding.lvCurrentDensity, 3);
    at("X17", result.winding.hvMeanTurn_m, 4);
    at("AH17", result.winding.lvMeanTurn_m, 4);
    at("W18", result.winding.hvWireLenMax_m, 2);
    at("Z18", result.winding.hvWireLenRated_m, 2);
    at("AH18", result.winding.lvWireLen_m, 2);
    at("X19", result.winding.hvResistance_ohm, 6);
    at("AH19", result.winding.lvResistance_ohm, 6);
    at("Y20", result.winding.hvCopperLoss_W, 1);
    at("AH20", result.winding.lvCopperLoss_W, 1);
    at("W21", result.winding.hvBareWireWeight_kg, 1);
    at("Z21", result.winding.hvWireWeight_kg, 1);
    at("AH21", result.winding.lvWireWeight_kg, 1);
    at("C10", result.winding.wireWeightTotal_kg, 1);
    at("L10", result.winding.loadLoss_W, 1);
    at("J10", result.winding.strayLossFactor, 3);
    at("AK23 B30", result.winding.lvRadial_mm, 1);
    at("W28 B39", result.winding.hvRadial_mm, 1);
    at("AA28", result.winding.hvAxial_mm, 2);
    at("AA29", result.winding.hvSegmentGap_mm, 2);
    at("AA30", result.winding.hvTotalAxial_mm, 2);
    at("AC30", result.winding.hvInnerAxial_mm, 2);
    at("AA31 AA32", result.winding.hvEndInsul_mm, 2);
    at("AC32", result.winding.hvEndInsulReference_mm, 0);
    at("AA33 J25", result.winding.lvAxial_mm, 2);
    at("AK43 B37", result.winding.mainDuct_mm, 2);
    at("AA45", result.winding.hvExtraLossPct, 2);
    at("AC45", result.winding.hvExtraLoss_W, 1);
    at("J14", result.mass.windowHeight_mm, 2);
    at("L14 B49", result.mass.centerDistance_mm, 2);
    at("C11", result.mass.activePartWeight_kg, 1);
    at("C19", result.mass.tankWeight_kg, 1);
    at("C24", result.mass.oilWeight_kg, 1);
    at("C25", result.mass.totalWeight_kg, 1);
    at("F25 N24", result.mass.tankWidth_mm, 1);
    at("H26 L26", result.mass.tankLength_mm, 1);
    at("J27 O24", result.mass.tankHeight_mm, 1);
    at("Q30", result.impedance.axialDifference_mm, 2);
    at("Q32", result.impedance.kx, 2);
    at("Q33", result.impedance.hx_mm, 1);
    at("Q34", result.impedance.resistanceDrop_pct, 2);
    at("Q35 R10", result.impedance.impedance_pct, 2);
    at("M39", result.impedance.lambda_mm, 2);
    at("R40", result.impedance.a2, 2);
    at("R41", result.impedance.a1, 2);
    at("O42", result.impedance.leakArea_mm2, 2);
    at("P43", result.impedance.reactanceDrop_pct, 2);
    at("O44", result.thermal.tankSurface_m2, 3);
    at("O45", result.thermal.corrSurface_m2, 3);
    at("O47", result.thermal.topSurface_m2, 3);
    at("O48", result.thermal.totalSurface_m2, 3);
    at("N49", result.thermal.oilRise_K, 2);
    at("N51", result.thermal.oilTopRise_K, 2);
    at("Y54", result.thermal.hvWindingRise_K, 2);
    at("AK52", result.thermal.lvWindingRise_K, 2);
    at("AC47", result.thermal.hvSurface_m2, 3);
    at("AK47", result.thermal.lvSurface_m2, 3);
    at("AB48", result.thermal.hvHeatLoad, 2);
    at("AK48", result.thermal.lvHeatLoad, 2);
    at("AC49", result.thermal.hvSurfaceRise_K, 2);
    at("AC51", result.thermal.hvGapCorrection_K, 2);
    at("AC52", result.thermal.hvLayerCorrection_K, 2);
    at("Y53", result.thermal.hvRiseAboveOil_K, 2);
    at("AK49", result.thermal.lvSurfaceRise_K, 2);
    at("AK50", result.thermal.lvLayerCorrection_K, 2);
    at("AK51", result.thermal.lvRiseAboveOil_K, 2);
    at("AG43", result.thermal.mainDuctSecond_mm, 2);
    const char *widthCells[] = {"G17","H17","I17","J17","K17","L17","M17","N17","O17","Q17","S17","F19","G19","H19","I19"};
    const char *stackCells[] = {"G18","H18","I18","J18","K18","L18","M18","N18","O18","Q18","S18","F20","G20","H20","I20"};
    for (int i = 0; i < 15; ++i) {
        if (i < result.core.widths_mm.size()) at(widthCells[i], result.core.widths_mm[i], 0);
        if (i < result.core.stacks_mm.size()) at(stackCells[i], result.core.stacks_mm[i], 2);
    }
    for (int i = 0; i < 5; ++i) at(qPrintable(QStringLiteral("Z%1").arg(30+i)), result.winding.ductLayerIdx[i], 0);
    original.append({QStringLiteral("C7"), result.testVoltage.highVoltage, QString()});
    original.append({QStringLiteral("C8"), result.testVoltage.lowVoltage, QString()});
    if (result.oilExpansion.available) {
        at("N53", result.oilExpansion.demand_kg, 2);
        at("N54", result.oilExpansion.capacity_kg, 2);
        at("S48", result.oilExpansion.longSideCount, 0);
        at("S49", result.oilExpansion.shortSideCount, 0);
        at("S51", result.oilExpansion.kp, 3);
    } else {
        for (const char *address : {"N53","N54","S48","S49","S51"})
            original.append({QString::fromLatin1(address), QStringLiteral("不可用"), QString()});
    }
    const QStringList detailAddresses = QStringLiteral(
        "W9 W10 W11 AA22 AA24 AA25 AA26 W24 W25 W26 X41 Z41 Y42 X42 Y41 Z42 "
        "AB36 AB39 AK33 AK34 AK36 AK39 AF10 AG10 AH10 AI10 AJ10 "
        "D51 D52 J24 B27 B29 B36 B38 B45 B47 H24 S44 C17 M29 M30 M31 M32 M33 M34 M35 M36 M37 M38 "
        "C12 C13 Q22 C14 C15 D12 D13 D14 D15 D16 D17 Q26 C20 C21 C22 O46 I49 S50 "
        "AA9 AA10 Y11 AA11 AK10 H10 I10 L11 M11 O11 R11 "
        "D53 F23 H23 S23 Q25 B48 B31 B32 B33 B34 B35 B40 B41 B42 B43 B44 "
        "W27 AA27 Q29 Q31 Z35 V41 V42 AB41 AB42 AC40 AC42 AH43 AI43 AJ43 L51 N50 AK12").split(QLatin1Char(' '))
        + CalculationSheetDescriptions::addresses();
    for (const auto &address : detailAddresses) {
        const auto it = result.calculationSheetDetails.constFind(address);
        const auto textIt = result.calculationSheetTexts.constFind(address);
        const QString note = result.calculationSheetNotes.value(address);
        const bool hasValue = result.sheetDetailsSnapshotAvailable && it != result.calculationSheetDetails.cend();
        const bool hasText = result.sheetDetailsSnapshotAvailable && textIt != result.calculationSheetTexts.cend();
        const QString value = hasText ? textIt.value()
            : result.sheetDetailsSnapshotAvailable && it != result.calculationSheetDetails.cend()
            ? snapshotValue(it.value().first, it.value().second)
            : result.sheetDetailsSnapshotAvailable && !note.isEmpty() ? note
            : result.sheetDetailsSnapshotAvailable && (address == QStringLiteral("AG10") || address == QStringLiteral("AH10")
                || address == QStringLiteral("AI10") || address == QStringLiteral("AJ10"))
                ? QStringLiteral("不适用") : QStringLiteral("待重新计算");
        const bool deviation = address == QStringLiteral("L11") || address == QStringLiteral("M11")
            || address == QStringLiteral("O11") || address == QStringLiteral("R11");
        original.append({address, value, hasValue && deviation ? QStringLiteral("%") : QString(), note});
    }
    groups.append({QStringLiteral("原单"), original});
    // 预留条件分支字段，使圆线/油膨缩状态变化时无需重建正在编辑的表。
    for (auto &group : groups) {
        QStringList optional;
        if (group.first == QStringLiteral("绕组")) optional = {QStringLiteral("圆线裸直径"), QStringLiteral("圆线表绝缘增重"),
            QStringLiteral("圆线附加损耗说明"), QStringLiteral("圆线表适用说明")};
        if (group.first == QStringLiteral("油膨缩校核")) optional = {QStringLiteral("采用总油重 C24"), QStringLiteral("膨胀需求 N53"),
            QStringLiteral("膨缩能力 N54"), QStringLiteral("膨缩能力余量"), QStringLiteral("波纹深 S45"),
            QStringLiteral("波纹高 S46"), QStringLiteral("波纹长边数量 S48"), QStringLiteral("波纹短边数量 S49"), QStringLiteral("波纹系数 Kp S51")};
        for (const auto &name : optional) {
            const bool exists = std::any_of(group.second.cbegin(), group.second.cend(), [&name](const QStringList &row) { return row.value(0) == name; });
            if (!exists) group.second.append({name, group.first == QStringLiteral("绕组") ? QStringLiteral("不适用") : QStringLiteral("不可用"), QString()});
        }
        if (!result.valid) for (auto &row : group.second) { row[1] = QStringLiteral("未计算"); row[2] = QString(); }
    }
    return groups;
}
int inputZone(const QString &section, const QString &name)
{
    if (section.startsWith(QStringLiteral("一 "))) return Basic;
    if (section.startsWith(QStringLiteral("二 ")) || section.startsWith(QStringLiteral("九 "))) return Performance;
    if (section.startsWith(QStringLiteral("三 "))) return Thermal;
    if (section.startsWith(QStringLiteral("四 ")) || section.startsWith(QStringLiteral("七 "))) return Core;
    if (section.startsWith(QStringLiteral("十 "))) return Tank;
    if (name.startsWith(QStringLiteral("低压")) || name.startsWith(QStringLiteral("推荐低压"))) return Low;
    if (name.startsWith(QStringLiteral("高压"))) return High;
    // 五节中未重复“高压”前缀的圆线规格、增厚、层间绝缘仍属高压输入。
    if (section.startsWith(QStringLiteral("五 "))) return High;
    return Duct;
}
int resultZone(const QString &group, const QString &name)
{
    if (group == QStringLiteral("基础电量")) return Basic;
    if (group == QStringLiteral("铁芯")) return name.contains(QStringLiteral("空载")) ? Performance : Core;
    if (group == QStringLiteral("温升")) return Thermal;
    if (group == QStringLiteral("阻抗电压")) return Duct;
    if (group == QStringLiteral("重量与成本")) return Tank;
    if (group == QStringLiteral("油膨缩校核")) return Reference;
    if (name.contains(QStringLiteral("负载损耗")) || name.contains(QStringLiteral("引线")) || name.contains(QStringLiteral("杂散"))) return Performance;
    if (name.startsWith(QStringLiteral("低压")) || name.startsWith(QStringLiteral("推荐低压"))) return Low;
    if (name.startsWith(QStringLiteral("高压")) || name.startsWith(QStringLiteral("实际高压"))
        || name.startsWith(QStringLiteral("端绝缘")) || name.startsWith(QStringLiteral("圆线"))) return High;
    return Duct;
}
}

SchemeCalculationSheet::SchemeCalculationSheet(const TransformerParams &params,
        const StructureConfig &config, const CalcInput &input, const CalcResult &result,
        bool proMode, QWidget *parent)
    : QWidget(parent), m_snapshot(result)
{
    const QStringList shapes = {QStringLiteral("圆形"),QStringLiteral("长圆形"),QStringLiteral("椭圆形"),QStringLiteral("半椭圆形"),QStringLiteral("类椭圆形")};
    m_coreShapeName = shapes.value(int(config.coreShape),QStringLiteral("未识别"));
    m_coolingName = config.category == StructureConfig::OilImmersed ? QStringLiteral("ONAN") : QStringLiteral("干式（未支持）");
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(3);
    m_resultStatus = new QLabel(this);
    m_resultStatus->setWordWrap(true);
    m_resultStatus->setStyleSheet(QStringLiteral("color:#185C37; padding:3px;"));
    outer->addWidget(m_resultStatus);
    // 唯一输入数据源保持原行列、校验、控件和联动，仅隐藏其旧布局。
    m_input = new ParamTableWidget(this);
    // The complete worksheet exposes existing advanced parameters in-place.
    Q_UNUSED(proMode);
    m_input->loadParamsForConfig(params, config, input, true);
    m_input->hide();
    m_results = new QTableWidget(this);
    m_results->setItemDelegate(new LiveInputDelegate(m_results));
    m_results->setColumnCount(12);
    m_results->horizontalHeader()->hide();
    m_results->verticalHeader()->setDefaultSectionSize(28);
    m_results->verticalHeader()->setMinimumSectionSize(26);
    m_results->setWordWrap(false); m_results->setTextElideMode(Qt::ElideRight);
    m_results->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_results->setStyleSheet(QStringLiteral(
        "QTableWidget {background:white; color:#24362B; gridline-color:#C9D8CE; border:1px solid #B8CEBF;"
        " selection-background-color:#CDE8D5; selection-color:#185C37;}"
        "QHeaderView::section {background:#E7F2EA; color:#185C37; border:1px solid #D3DDD6; padding:2px;}"
        "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox {background:#F0FAF2; color:#24362B; border:1px solid #A8CDB3; padding:1px;}"
        "QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus {border:1px solid #217346;}"
        "QComboBox QAbstractItemView {background:white; color:#24362B; selection-background-color:#CDE8D5;}"));
    for (int col = 0; col < 12; ++col) {
        // 等宽列构成三个等宽分区，随最大化/还原自动铺满，不留下右侧空白。
        m_results->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    }
    m_results->horizontalHeader()->setMinimumSectionSize(30);
    outer->addWidget(m_results, 1);
    connect(m_results, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *cell) {
        for (const auto &b : m_bindings) {
            if (b.cell != cell) continue;
            auto *source = m_input->item(b.row, b.col);
            if (source && source->flags().testFlag(Qt::ItemIsEditable) && !m_input->cellWidget(b.row, b.col)) source->setText(cell->text());
            break;
        }
    });
    connect(m_input, &QTableWidget::itemChanged, this, [this]() { syncInputs(); markPending(); });
    for (auto *edit : m_input->findChildren<QLineEdit *>()) connect(edit, &QLineEdit::textEdited, this, [this]() { syncInputs(); markPending(); });
    for (auto *combo : m_input->findChildren<QComboBox *>()) connect(combo, &QComboBox::currentIndexChanged, this, [this]() { syncInputs(); markPending(); });
    for (auto *spin : m_input->findChildren<QSpinBox *>()) connect(spin, &QSpinBox::valueChanged, this, [this]() { syncInputs(); markPending(); });
    for (auto *spin : m_input->findChildren<QDoubleSpinBox *>()) connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() { syncInputs(); markPending(); });
    loadResult(result);
}

QWidget *SchemeCalculationSheet::mirrorEditor(QWidget *source)
{
    QWidget *copy = nullptr;
    if (auto *s = qobject_cast<QComboBox *>(source)) {
        auto *d = new QComboBox(m_results);
        d->setModel(s->model()); d->setModelColumn(s->modelColumn()); d->setCurrentIndex(s->currentIndex());
        connect(d, &QComboBox::currentIndexChanged, s, [s](int index) { s->setCurrentIndex(index); });
        connect(s, &QComboBox::currentIndexChanged, d, [s,d]() { const QSignalBlocker block(d); d->setCurrentIndex(s->currentIndex()); });
        copy = d;
    } else if (auto *s = qobject_cast<QDoubleSpinBox *>(source)) {
        auto *d = new QDoubleSpinBox(m_results);
        d->setAlignment(Qt::AlignRight);
        d->setDecimals(s->decimals()); d->setRange(s->minimum(), s->maximum()); d->setSingleStep(s->singleStep());
        d->setPrefix(s->prefix()); d->setSuffix(s->suffix()); d->setValue(s->value());
        connect(d, &QDoubleSpinBox::valueChanged, s, [s](double value) { s->setValue(value); });
        connect(s, &QDoubleSpinBox::valueChanged, d, [s,d]() { const QSignalBlocker block(d); d->setValue(s->value()); });
        copy = d;
    } else if (auto *s = qobject_cast<QSpinBox *>(source)) {
        auto *d = new QSpinBox(m_results);
        d->setAlignment(Qt::AlignRight);
        d->setRange(s->minimum(), s->maximum()); d->setSingleStep(s->singleStep());
        d->setPrefix(s->prefix()); d->setSuffix(s->suffix()); d->setValue(s->value());
        connect(d, &QSpinBox::valueChanged, s, [s](int value) { s->setValue(value); });
        connect(s, &QSpinBox::valueChanged, d, [s,d]() { const QSignalBlocker block(d); d->setValue(s->value()); });
        copy = d;
    } else if (auto *s = qobject_cast<QLineEdit *>(source)) {
        auto *d = new QLineEdit(s->text(), m_results);
        d->setReadOnly(s->isReadOnly()); d->setPlaceholderText(s->placeholderText()); d->setValidator(s->validator());
        connect(d, &QLineEdit::textEdited, s, [s](const QString &text) {
            s->setText(text);
            // 原型号联动监听textEdited，转交真实用户编辑，而非伪造新的解析逻辑。
            QMetaObject::invokeMethod(s, "textEdited", Qt::DirectConnection, Q_ARG(QString, text));
        });
        connect(d, &QLineEdit::returnPressed, s, [this,s]() {
            QMetaObject::invokeMethod(s, "returnPressed", Qt::DirectConnection);
            syncInputs(); // 低压额定电压在回车时提交，原函数可能屏蔽table信号。
            markPending();
        });
        connect(s, &QLineEdit::textChanged, d, [s,d]() {
            if (d->text() != s->text()) { const QSignalBlocker block(d); d->setText(s->text()); }
        });
        copy = d;
    } else if (auto *s = qobject_cast<QLabel *>(source)) {
        copy = new QLabel(s->text(), m_results);
    } else if (source->layout()) {
        copy = new QWidget(m_results);
        auto *layout = new QHBoxLayout(copy);
        layout->setContentsMargins(1, 0, 1, 0); layout->setSpacing(1);
        for (int i = 0; i < source->layout()->count(); ++i) {
            if (auto *child = source->layout()->itemAt(i)->widget()) {
                if (auto *mirror = mirrorEditor(child)) layout->addWidget(mirror);
            }
        }
    }
    if (copy) {
        if (auto *spin = qobject_cast<QAbstractSpinBox *>(copy)) {
            if (auto *edit = spin->findChild<QLineEdit *>())
                connect(edit, &QLineEdit::textEdited, this, [this]() { markPending(); });
        }
        copy->setMinimumWidth(0); copy->setToolTip(source->toolTip());
        copy->setEnabled(source->isEnabled()); copy->setVisible(!source->isHidden());
        m_editors.insert(source, copy); source->installEventFilter(this);
    }
    return copy;
}

bool SchemeCalculationSheet::eventFilter(QObject *source, QEvent *event)
{
    if (auto *copy = m_editors.value(source).data()) {
        auto *widget = qobject_cast<QWidget *>(source);
        if (widget && (event->type() == QEvent::EnabledChange || event->type() == QEvent::Show
                || event->type() == QEvent::Hide || event->type() == QEvent::ToolTipChange)) {
            copy->setEnabled(widget->isEnabled()); copy->setVisible(!widget->isHidden()); copy->setToolTip(widget->toolTip());
        }
    }
    return QWidget::eventFilter(source, event);
}

void SchemeCalculationSheet::syncInputs()
{
    const QSignalBlocker blocker(m_results);
    const auto show = [this](int row, int col, const QString &text) {
        if (auto *cell = m_results->item(row,col)) {
            cell->setText(text);
            cell->setForeground(QColor("#24362B"));
            cell->setToolTip(text);
        }
    };
    const auto params = m_input->getParams();
    show(3,2,QStringLiteral("3"));
    show(3,5,m_coreShapeName);
    show(16,5,m_coreShapeName);
    show(4,5,QStringLiteral("T形轭"));
    show(5,2,m_coolingName);
    show(20,12,QStringLiteral("波纹油箱"));
    const auto *hvMaterial = qobject_cast<QComboBox *>(m_results->cellWidget(12,20));
    const auto *lvMaterial = qobject_cast<QComboBox *>(m_results->cellWidget(12,29));
    if (hvMaterial && lvMaterial) {
        const bool copperHv = hvMaterial->currentText() == QStringLiteral("铜");
        const bool copperLv = lvMaterial->currentText() == QStringLiteral("铜箔");
        show(5,5,copperHv == copperLv ? (copperHv ? QStringLiteral("全铜") : QStringLiteral("全铝"))
            : (copperHv ? QStringLiteral("高铜低铝") : QStringLiteral("高铝低铜")));
    }
    show(2,5,params.standardMode == TransformerParams::StandardMode::Custom ? QStringLiteral("非标")
        : params.standardMode == TransformerParams::StandardMode::BuiltIn ? QStringLiteral("内置标准") : QStringLiteral("待确认"));
    show(2,24,params.connectionGroup.startsWith(QLatin1Char('D'),Qt::CaseInsensitive) ? QStringLiteral("D") : QStringLiteral("Y"));
    show(2,33,params.connectionGroup.contains(QStringLiteral("yn"),Qt::CaseInsensitive) ? QStringLiteral("yn") : QStringLiteral("d"));
    // 原联动有意用QSignalBlocker改变控件；此处也同步被屏蔽信号的选择值。
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *copy = it.value().data();
        if (!copy) continue;
        const QSignalBlocker block(copy);
        if (auto *s = qobject_cast<QComboBox *>(it.key()))
            qobject_cast<QComboBox *>(copy)->setCurrentIndex(s->currentIndex());
        else if (auto *s = qobject_cast<QLineEdit *>(it.key())) {
            auto *d = qobject_cast<QLineEdit *>(copy);
            if (d->text() != s->text()) d->setText(s->text());
        }
    }
    for (const auto &b : m_bindings) {
        const auto *source = m_input->item(b.row, b.col);
        if (!source) continue;
        b.cell->setText(source->text());
        bool numeric = false;
        source->text().toDouble(&numeric);
        b.cell->setTextAlignment((numeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        auto flags = source->flags();
        if (m_input->cellWidget(b.row, b.col)) flags &= ~Qt::ItemIsEditable;
        b.cell->setFlags(flags);
        b.cell->setBackground(QColor(source->flags().testFlag(Qt::ItemIsEditable) ? "#F0FAF2" : "#FFFFFF"));
        b.cell->setToolTip(source->toolTip().isEmpty() ? source->text() : source->toolTip());
    }
}

void SchemeCalculationSheet::rebuildSheet()
{
    QVector<Field> fields[ZoneCount];
    QString section;
    for (int row = 0; row < m_input->rowCount(); ++row) {
        const auto *left = m_input->item(row, 1);
        if (!left) continue;
        const bool heading = left->font().bold();
        if (heading) section = left->text();
        for (int col : {2, 3, 4}) {
            const auto *value = m_input->item(row, col);
            if (!value) continue;
            if (heading && !value->flags().testFlag(Qt::ItemIsEditable)) continue;
            if (col != 2 && m_input->columnSpan(row, 2) > 1) continue;
            if (col == 3 && !value->flags().testFlag(Qt::ItemIsEditable)) continue;
            if (value->text().isEmpty() && !value->flags().testFlag(Qt::ItemIsEditable) && !m_input->cellWidget(row, col)) continue;
            QString name = col == 2 ? left->text() : m_input->item(row, 3)->text();
            if (col == 3) name = left->text() + QStringLiteral(" 最小偏差(%)");
            else if (col == 4 && (name.isEmpty() || m_input->item(row, 3)->flags().testFlag(Qt::ItemIsEditable))) name = left->text() + QStringLiteral(" 最大偏差(%)");
            QString tip = name + QLatin1Char('\n') + value->toolTip();
            if (auto *note = m_input->item(row, 5)) tip += QLatin1Char('\n') + note->text() + QLatin1Char('\n') + note->toolTip();
            fields[inputZone(section, name)].append({name, value->text(), tip, row, col, m_input->columnSpan(row, col) > 1});
        }
    }
    {
        for (const auto &group : sheetGroups(m_snapshot)) {
            for (const auto &values : group.second) {
                const QString value = values.value(1) + (values.value(2).isEmpty() ? QString() : QLatin1Char(' ') + values.value(2));
                QString displayName = values.value(0);
                if (group.first == QStringLiteral("铁芯")) {
                    if (displayName == QStringLiteral("心柱截面")) displayName = QStringLiteral("心柱几何截面 F14");
                    else if (displayName == QStringLiteral("铁轭截面")) displayName = QStringLiteral("铁轭几何截面（Sheet1!J23）");
                    else if (displayName == QStringLiteral("心柱磁密")) displayName = QStringLiteral("心柱设计磁密 F16");
                    else if (displayName == QStringLiteral("铁轭磁密")) displayName = QStringLiteral("铁轭查表磁密（Sheet1!O25）");
                    else if (displayName == QStringLiteral("心柱实际截面 I24")) displayName = QStringLiteral("心柱实际截面（Sheet1!I24）");
                    else if (displayName == QStringLiteral("心柱实际查表磁密 I25")) displayName = QStringLiteral("心柱实际查表磁密（Sheet1!I25）");
                    else if (displayName == QStringLiteral("铁轭实际截面 O24")) displayName = QStringLiteral("铁轭实际截面（Sheet1!O24）");
                    else if (displayName == QStringLiteral("心柱分项重量 P23")) displayName = QStringLiteral("心柱分项重量（Sheet1!P23）");
                    else if (displayName == QStringLiteral("铁轭分项重量 U23")) displayName = QStringLiteral("铁轭分项重量（Sheet1!U23）");
                }
                QString tip = displayName + QStringLiteral("\n计算值（只读）：") + value;
                if (!values.value(3).isEmpty()) tip += QLatin1Char('\n') + values.value(3);
                if (displayName.contains(QStringLiteral("几何叠厚")))
                    tip += QStringLiteral("\n几何叠厚来自当前叠积快照，不等同于片数取整后的实际叠厚。");
                fields[resultZone(group.first, values.value(0))].append({displayName, value,
                    tip, -1, -1, false,
                    group.first + QLatin1Char('\n') + values.value(0)});
            }
        }
    }
    const int scroll = m_results->verticalScrollBar()->value();
    const QSignalBlocker blocker(m_results);
    m_bindings.clear(); m_editors.clear(); m_resultBindings.clear();
    m_results->clearSpans(); m_results->setRowCount(0);
    m_results->setColumnCount(38); m_results->setRowCount(54);
    const auto layout = QJsonDocument::fromJson(calculationSheetTemplate).object();
    const auto widths = layout.value(QStringLiteral("widths")).toArray();
    for (int col = 0; col < 38; ++col) {
        m_results->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Fixed);
        m_results->setColumnWidth(col, std::max(34, int(widths[col].toDouble() * 10)));
    }
    const auto cell = [&](int row, int col, const QString &text, const QString &tip = QString()) {
        auto *item = new QTableWidgetItem(text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setTextAlignment(Qt::AlignCenter);
        item->setToolTip(tip);
        m_results->setItem(row, col, item);
        return item;
    };
    const auto position = [](const QString &address) {
        int col = 0, index = 0;
        while (index < address.size() && address[index].isLetter())
            col = col * 26 + address[index++].unicode() - 'A' + 1;
        return QPair<int,int>(address.mid(index).toInt()-1, col-1);
    };
    for (const auto &entry : layout.value(QStringLiteral("cells")).toArray()) {
        const auto a = entry.toArray();
        const QString address = a[0].toString();
        auto *item = cell(a[1].toInt(), a[2].toInt(),
            a[4].toBool() ? QStringLiteral("—") : a[3].toString(),
            QStringLiteral("计算单!%1").arg(address));
        if (a[4].toBool()) {
            item->setForeground(QColor("#8A6A30"));
            item->setToolTip(item->toolTip() + QStringLiteral("\n此原表字段尚未接入当前方案数据，不能用于计算或判定。"));
        }
    }
    for (const auto &entry : layout.value(QStringLiteral("merges")).toArray()) {
        const auto a = entry.toArray();
        if (a[0].toInt() == 2 && a[1].toInt() >= 9) continue;
        m_results->setSpan(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt());
    }
    // Composite model and asymmetric taps need one continuous editing area.
    m_results->setSpan(0,0,2,5); cell(0,0,QStringLiteral("产品型号"));
    m_results->setSpan(0,5,2,14);
    m_results->setSpan(0,19,2,19); cell(0,19,QStringLiteral("绕组计算单（高压 / 低压）"));
    m_results->setSpan(2,2,1,2);
    m_results->setSpan(2,9,1,10);
    for (int c = 10; c < 19; ++c) if (auto *item = m_results->item(2,c)) item->setText(QString());
    // P12/S12 are named by the source workbook; engine member names are historical.
    cell(11,13,QStringLiteral("小圆心角")); cell(11,16,QStringLiteral("大圆心角"));
    cell(11,19,QStringLiteral("总层数 W12"));
    cell(17,5,QStringLiteral("几何值"),QStringLiteral("几何叠厚；不等同于片数取整后的实际叠厚"));
    cell(22,29,QStringLiteral("箔厚×匝数＋层间绝缘（取整后）"));
    cell(24,30,QStringLiteral("箔宽＋两端绝缘"));
    for (int i=0; i<5; ++i)
        cell(28+i,4,QStringLiteral("DE2%1　低压第%1油道折算厚").arg(i+1));
    for (int i=0; i<5; ++i)
        cell(33+i,4,QStringLiteral("DE1%1　高压第%1油道折算厚").arg(i+1));
    cell(9,9,QStringLiteral("—"), QStringLiteral("杂散损耗系数 J10，绑定后显示当前值"));
    QSet<QString> usedInputs;
    QSet<QString> representedAddresses;
    const auto bind = [&](int row, int col, const Field &f, bool withName) {
        auto *value = cell(row,col,f.value,f.tip);
        value->setForeground(QColor("#24362B"));
        value->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (!f.key.isEmpty()) m_resultBindings.insert(f.key,value);
        if (f.row >= 0) {
            usedInputs.insert(QStringLiteral("%1/%2").arg(f.row).arg(f.col));
            m_bindings.append({f.row,f.col,value});
            if (auto *source = m_input->cellWidget(f.row,f.col))
                if (auto *editor = mirrorEditor(source)) m_results->setCellWidget(row,col,editor);
        }
        if (!withName) value->setToolTip(QStringLiteral("%1\n%2").arg(f.name, f.tip));
    };
    const auto inputAt = [&](const QString &address, const QString &name) {
        for (int zone=0; zone<ZoneCount; ++zone)
            for (const auto &f : fields[zone]) {
                if (f.row < 0 || f.name != name) continue;
                const auto p = position(address);
                bind(p.first,p.second,f,false);
                representedAddresses.insert(address);
                return;
            }
    };
    inputAt(QStringLiteral("F1"),QStringLiteral("产品型号"));
    // Taps use the same three original controls and validation.
    for (int zone=0; zone<ZoneCount; ++zone)
        for (const auto &f : fields[zone])
            if (f.row >= 0 && f.name.contains(QStringLiteral("调压"))) {
                bind(2,9,f,false); break;
            }
    inputAt(QStringLiteral("C3"),QStringLiteral("联结组别"));
    inputAt(QStringLiteral("C5"),QStringLiteral("频率(Hz)"));
    inputAt(QStringLiteral("H12"),QStringLiteral("铁芯直径(mm)"));
    inputAt(QStringLiteral("K12"),QStringLiteral("直线段长(mm)"));
    inputAt(QStringLiteral("M12"),QStringLiteral("椭圆角(°)"));
    inputAt(QStringLiteral("P14"),QStringLiteral("硅钢片牌号"));
    inputAt(QStringLiteral("L16"),QStringLiteral("叠片系数"));
    inputAt(QStringLiteral("H16"),QStringLiteral("原单单位铁损记录(W/kg)"));
    inputAt(QStringLiteral("M16"),QStringLiteral("原单旧工艺系数记录"));
    inputAt(QStringLiteral("L9"),QStringLiteral("负载损耗标准值(W)"));
    inputAt(QStringLiteral("M9"),QStringLiteral("空载损耗标准值(W)"));
    inputAt(QStringLiteral("O9"),QStringLiteral("空载电流标准值(%)"));
    inputAt(QStringLiteral("R9"),QStringLiteral("阻抗电压标准值(%)"));
    inputAt(QStringLiteral("U13"),QStringLiteral("高压导线材料"));
    inputAt(QStringLiteral("AD13"),QStringLiteral("低压箔材料"));
    inputAt(QStringLiteral("X13"),QStringLiteral("高压裸线宽(mm)"));
    inputAt(QStringLiteral("Z13"),QStringLiteral("高压裸线厚(mm)"));
    inputAt(QStringLiteral("AC13"),QStringLiteral("高压导线形状"));
    inputAt(QStringLiteral("U14"),QStringLiteral("高压导线绝缘种类"));
    inputAt(QStringLiteral("AB13"),QStringLiteral("高压并绕根数"));
    inputAt(QStringLiteral("AB14"),QStringLiteral("高压叠绕根数"));
    inputAt(QStringLiteral("W12"),QStringLiteral("高压总层数 W12"));
    inputAt(QStringLiteral("AH8"),QStringLiteral("低压匝数"));
    inputAt(QStringLiteral("AG12"),QStringLiteral("低压内连接排厚(mm)"));
    inputAt(QStringLiteral("AJ12"),QStringLiteral("低压内连接排宽(mm)"));
    inputAt(QStringLiteral("AF14"),QStringLiteral("低压箔厚(mm)"));
    inputAt(QStringLiteral("AJ14"),QStringLiteral("低压箔宽(mm)"));
    inputAt(QStringLiteral("AI25"),QStringLiteral("低压端绝缘(mm)"));
    inputAt(QStringLiteral("X35"),QStringLiteral("层间绝缘厚(mm)"));
    inputAt(QStringLiteral("AG26"),QStringLiteral("低压层间绝缘层数"));
    inputAt(QStringLiteral("AI26"),QStringLiteral("低压层间绝缘厚(mm)"));
    inputAt(QStringLiteral("J10"),QStringLiteral("杂散损耗系数"));
    inputAt(QStringLiteral("S45"),QStringLiteral("波纹深(mm)"));
    inputAt(QStringLiteral("S46"),QStringLiteral("波纹高(mm)"));
    inputAt(QStringLiteral("S47"),QStringLiteral("波纹节距(mm)"));
    inputAt(QStringLiteral("F24"),QStringLiteral("器身侧净空(mm)"));
    inputAt(QStringLiteral("H25"),QStringLiteral("器身端净空(mm)"));
    inputAt(QStringLiteral("J23"),QStringLiteral("箱底油空(mm)"));
    inputAt(QStringLiteral("J26"),QStringLiteral("垫脚高(mm)"));
    inputAt(QStringLiteral("AD43"),QStringLiteral("主空道宽(mm)"));
    inputAt(QStringLiteral("AF43"),QStringLiteral("纸板厚(mm)"));
    inputAt(QStringLiteral("B28"),QStringLiteral("相间距基础(mm)"));
    for (int i=0; i<5; ++i) {
        inputAt(QStringLiteral("X%1").arg(30+i),QStringLiteral("高压油道宽 %1(mm)").arg(i+1));
        inputAt(QStringLiteral("Y%1").arg(30+i),QStringLiteral("高压油道高 %1(mm)").arg(i+1));
        inputAt(QStringLiteral("AG%1").arg(28+i),QStringLiteral("低压油道宽 %1(mm)").arg(i+1));
        inputAt(QStringLiteral("AH%1").arg(28+i),QStringLiteral("低压油道高 %1(mm)").arg(i+1));
    }
    for (int zone=0; zone<ZoneCount; ++zone)
        for (const auto &f : fields[zone]) {
            if (!f.key.startsWith(QStringLiteral("原单\n"))) continue;
            const auto p = position(f.key.section(QLatin1Char('\n'),1));
            bool occupied = false;
            for (const auto &b : m_bindings)
                if (b.cell->row()==p.first && b.cell->column()==p.second) occupied=true;
            if (!occupied) bind(p.first,p.second,f,false);
            representedAddresses.insert(f.key.section(QLatin1Char('\n'),1));
        }
    QVector<Field> advanced, checks;
    const auto &owners = originalResultAddresses();
    for (int zone=0; zone<ZoneCount; ++zone) {
        for (const auto &f : fields[zone]) {
            if (f.key.startsWith(QStringLiteral("原单\n"))) continue;
            if (f.row >= 0 && usedInputs.contains(QStringLiteral("%1/%2").arg(f.row).arg(f.col))) continue;
            const auto owner = owners.constFind(f.key);
            if (owner != owners.cend() && representedAddresses.contains(owner.value())) continue;
            bool stackShown = false;
            for (int i=1; i<=15; ++i)
                if (f.name == QStringLiteral("第%1级片宽 / 几何叠厚").arg(i)) stackShown = true;
            if (stackShown) continue;
            const bool craftInput = f.row >= 0 && zone != Basic && zone != Performance && zone != Thermal;
            if (craftInput) advanced.append(f);
            else checks.append(f);
        }
    }
    // Collapse only hides rows: source bindings, editor instances and calculation
    // remain alive, so toggling never rebuilds the sheet or discards user edits.
    int row=54;
    const auto addSection = [&](const QString &title, const QVector<Field> &items) {
        if (items.isEmpty()) return;
        const int headerRow=row++;
        m_results->setRowCount(row);
        m_results->setSpan(headerRow,0,1,38);
        auto *header = cell(headerRow,0,QString());
        header->setBackground(QColor("#DCEEDF"));
        auto *button = new QToolButton(m_results);
        button->setText(title + QStringLiteral("（%1项，点击展开）").arg(items.size()));
        button->setCheckable(true);
        button->setArrowType(Qt::RightArrow);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setStyleSheet(QStringLiteral("QToolButton {background:#DCEEDF; color:#185C37; border:0; text-align:left; padding:4px; font-weight:bold;}"));
        m_results->setCellWidget(headerRow,0,button);
        m_results->setRowHeight(headerRow,34);
        const int firstRow=row;
        int slot=0;
        for (const auto &f : items) {
            m_results->setRowCount(row+1);
            const int base=slot*19;
            cell(row,base,f.name,f.tip); m_results->setSpan(row,base,1,9);
            bind(row,base+9,f,true); m_results->setSpan(row,base+9,1,10);
            if (++slot==2) { slot=0; ++row; }
        }
        if (slot) ++row;
        const int lastRow=row;
        for (int r=firstRow; r<lastRow; ++r) m_results->setRowHidden(r,true);
        connect(button,&QToolButton::toggled,this,[this,button,title,count=items.size(),firstRow,lastRow](bool open) {
            button->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
            button->setText(title + QStringLiteral("（%1项，点击%2）").arg(count).arg(open ? QStringLiteral("收起") : QStringLiteral("展开")));
            for (int r=firstRow; r<lastRow; ++r) m_results->setRowHidden(r,!open);
        });
    };
    addSection(QStringLiteral("高级工艺参数"),advanced);
    addSection(QStringLiteral("校核指标与说明"),checks);
    for (int r=0; r<m_results->rowCount(); ++r)
        for (int c=0; c<38; ++c)
            if (!m_results->item(r,c)) cell(r,c,QString());
    for (int r : {6,7,8,9,10,11,12,13,14,15,22,24,32,33,35,38,42,43,44,46,47,48,49,50,51,52,53})
        m_results->setRowHeight(r,36);
    syncInputs(); m_results->verticalScrollBar()->setValue(scroll);
}

void SchemeCalculationSheet::markPending()
{
    m_resultStatus->setText(QStringLiteral("浅绿：可输入　白色：只读　输入已修改，正在自动更新；旧结果暂未刷新"));
    emit inputEdited();
}

bool SchemeCalculationSheet::inputsReady(QString &error) const
{
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *spin = qobject_cast<QAbstractSpinBox *>(it.value().data());
        if (spin && spin->isEnabled() && !spin->isHidden() && !spin->hasAcceptableInput()) {
            error = QStringLiteral("调压数值尚未输入完整或超出范围，请补全后再计算。");
            return false;
        }
    }
    return true;
}

void SchemeCalculationSheet::showCalculationError(const QString &error)
{
    m_resultStatus->setText(QStringLiteral("自动计算未更新（原结果仅供查看）：") + error);
}

void SchemeCalculationSheet::refreshResult(const CalcResult &result)
{
    m_snapshot = result;
    const QSignalBlocker blocker(m_results);
    for (const auto &group : sheetGroups(result)) {
        for (const auto &row : group.second) {
            const auto cells = m_resultBindings.values(group.first + QLatin1Char('\n') + row.value(0));
            for (auto *cell : cells) {
            const QString value = row.value(1) + (row.value(2).isEmpty() ? QString() : QLatin1Char(' ') + row.value(2));
            cell->setText(value); cell->setToolTip(row.value(0) + QStringLiteral("\n计算值（只读）：") + value
                + (row.value(3).isEmpty() ? QString() : QLatin1Char('\n') + row.value(3)));
            bool numeric = false; row.value(1).toDouble(&numeric);
            cell->setTextAlignment((numeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
            }
        }
    }
    m_resultStatus->setText(QStringLiteral("原计算单布局　浅绿：可输入　白色：只读　已按当前参数自动计算；— 表示该字段未接入，不能用于判定"));
}

void SchemeCalculationSheet::loadResult(const CalcResult &result)
{
    m_snapshot = result;
    rebuildSheet();
    m_resultStatus->setText(result.valid ? QStringLiteral("原计算单布局　浅绿：可输入　白色：只读　—：未接入当前方案数据（悬停查看）；下方两区可点击展开")
        : QStringLiteral("计算结果不可用，修改参数后自动计算；浅绿单元格可输入"));
}
