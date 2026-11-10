#include "SchemeCalculationSheet.h"
#include "ParamTableWidget.h"
#include "PerformanceCriteria.h"
#include "EmResultPanel.h"
#include "CalculationSheetTemplate.h"
#include "CalculationSheetDescriptions.h"
#include "TankDimensionsPanel.h"
#include "HvCoilFormNotes.h"
#include "ElectromagneticEngine.h"
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
#include <QCommonStyle>
#include <QPersistentModelIndex>
#include <QToolButton>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <algorithm>
#include <array>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QSaveFile>
#include "SchemeStore.h"
#include <QPrinter>
#include <QPainter>
#include <QLayout>

namespace {
enum Zone { Basic, Performance, Thermal, Core, High, Low, Duct, Tank, Reference, ZoneCount };
// Source T-column blocks are bounded by explicit Excel borders, not gridlines.
// Preserve each caption's source row even when the region is one large cell.
class WorksheetLabelBlock final : public QWidget {
public:
    WorksheetLabelBlock(QTableWidget *table, int first, int count, int column=19)
        : QWidget(table), m_table(table), m_first(first), m_count(count), m_column(column)
    {
        setProperty("worksheetLabelBlock", true);
        QStringList captions;
        for (int r=first;r<first+count;++r) {
            const auto *item=table->item(r,column);
            const QString text=item?item->text():QString();
            m_texts.append(text);
            if(!text.trimmed().isEmpty())captions.append(text);
        }
        setAccessibleName(captions.join(QStringLiteral("；")));
        connect(table,&QTableWidget::itemChanged,this,[this](QTableWidgetItem *){update();});
        setMinimumSize(0,0);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(),Qt::white);
        p.setFont(m_table->font());
        p.setPen(Qt::black);
        int total=0;
        for(int i=0;i<m_count;++i)total+=m_table->rowHeight(m_first+i);
        qreal y=0;
        for(int i=0;i<m_count;++i) {
            int sourceHeight=m_table->rowHeight(m_first+i);
            // Original T10:T11 centers its single caption over both rows.
            if(m_column==19&&m_first==8&&i==1)sourceHeight+=m_table->rowHeight(m_first+i+1);
            const qreal h=height()*qreal(sourceHeight)/qMax(1,total);
            const auto *item=m_table->item(m_first+i,m_column);
            const QString text=item?item->text():QString();
            if(!text.isEmpty())p.drawText(QRectF(2,y,width()-4,h),Qt::AlignCenter,text);
            y+=height()*qreal(m_table->rowHeight(m_first+i))/qMax(1,total);
        }
        p.setPen(QPen(Qt::black,1));
        p.drawRect(rect().adjusted(0,0,-1,-1));
    }
private:
    QTableWidget *m_table;
    int m_first,m_count,m_column;
    QStringList m_texts;
};

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
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if ((index.flags() & Qt::ItemIsEditable)
                && index.data(Qt::BackgroundRole).value<QBrush>().color() == QColor("#FF00FF")) {
            QStyleOptionViewItem local(option);
            initStyleOption(&local, index);
            // The application stylesheet's item rule hides model backgrounds.
            // Draw editable cells through Qt's unstyled item renderer instead.
            static QCommonStyle plainStyle;
            plainStyle.drawControl(QStyle::CE_ItemViewItem, &local, painter);
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
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
    at("V8", result.winding.hvTurnsMax, 0);
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
        // The direct calculation dialog starts without a result. Register every
        // cell now so the first automatic calculation can refresh it in place.
        if (i < result.core.widths_mm.size()) at(widthCells[i], result.core.widths_mm[i], 0);
        else original.append({QString::fromLatin1(widthCells[i]), QStringLiteral("—"), QString()});
        if (i < result.core.stacks_mm.size()) at(stackCells[i], result.core.stacks_mm[i], 2);
        else original.append({QString::fromLatin1(stackCells[i]), QStringLiteral("—"), QString()});
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
        "AA9 AA10 Y11 AA11 AA12 Y15 AK10 H10 I10 L11 M11 O11 R11 "
        "D53 F23 H23 S23 Q25 B48 B31 B32 B33 B34 B35 B40 B41 B42 B43 B44 "
        "W27 AA27 Q29 Q31 Z35 V41 V42 AB41 AB42 AC40 AC42 AH43 AI43 AJ43 L51 N50 AK12 K19 K20").split(QLatin1Char(' '))
        + CalculationSheetDescriptions::addresses();
    for (const auto &address : detailAddresses) {
        const auto it = result.calculationSheetDetails.constFind(address);
        const auto textIt = result.calculationSheetTexts.constFind(address);
        QString note = result.calculationSheetNotes.value(address);
        // Old saved schemes may contain the pre-relocation label in this note.
        if (address == QStringLiteral("AJ45"))
            note.replace(QStringLiteral("见下方高级工艺参数“低压附加损耗(W)”"),
                QStringLiteral("见顶部“补充参数”的“参数”页中“低压附加损耗(W)”"));
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
    // AN列在主表AL范围之外；只在下方校核区保留一个参考行，不扩展图形区域。
    const QString chordAddress = QStringLiteral("AN14");
    const auto chordIt = result.calculationSheetDetails.constFind(chordAddress);
    const bool chordAvailable = result.valid && result.sheetDetailsSnapshotAvailable;
    const bool chordNumber = chordAvailable && chordIt != result.calculationSheetDetails.cend();
    const QString chordValue = !chordAvailable ? QStringLiteral("待重新计算")
        : result.calculationSheetTexts.contains(chordAddress) ? result.calculationSheetTexts.value(chordAddress)
        : chordNumber ? snapshotValue(chordIt.value().first, chordIt.value().second) : QStringLiteral("待重新计算");
    groups.append({QStringLiteral("内连接排参考"), {{QStringLiteral("内排弦高（原单AN14，参考）"),
        chordValue, chordNumber ? QStringLiteral("mm") : QString(), result.calculationSheetNotes.value(chordAddress)}}});
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
    : QWidget(parent), m_snapshot(result), m_previewBaseInput(input)
{
    m_coreShapeName = StructureConfig::coreShapeName(input.coreShape);
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
    // Only expose the existing selectors, not the design page's extra preview rows.
    auto sheetConfig = config;
    if (input.hvCoilFormIdx == 1) sheetConfig.hvCoilStructure = StructureConfig::MultiLayerCylinder;
    else if (input.hvCoilFormIdx == 2) sheetConfig.hvCoilStructure = StructureConfig::TwoSegCylinder;
    m_input->loadParamsForConfig(params, sheetConfig, input, true, false, true);
    m_input->hide();
    m_results = new QTableWidget(this);
    m_results->viewport()->installEventFilter(this);
    m_results->setItemDelegate(new LiveInputDelegate(m_results));
    m_results->setColumnCount(12);
    m_results->horizontalHeader()->hide();
    m_results->verticalHeader()->setDefaultSectionSize(28);
    m_results->verticalHeader()->setMinimumSectionSize(26);
    m_results->setWordWrap(true); m_results->setTextElideMode(Qt::ElideNone);
    m_results->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_results->setStyleSheet(QStringLiteral(
        "QTableWidget {background:white; color:#24362B; font-size:13px; gridline-color:#C9D8CE; border:1px solid #B8CEBF;"
        " selection-background-color:#CDE8D5; selection-color:#185C37;}"
        "QHeaderView::section {background:#E7F2EA; color:#185C37; border:1px solid #D3DDD6; padding:2px;}"
        "QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox {background:#FF00FF; color:#24362B; border:1px solid #A8CDB3; padding:1px;}"
        "QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus {border:1px solid #217346;}"
        "QLineEdit[readOnly=\"true\"] {background:white;}"
        "QLineEdit:disabled,QComboBox:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled {background:white;}"
        "QComboBox QAbstractItemView {background:white; color:#24362B; selection-background-color:#CDE8D5;}"));
    for (int col = 0; col < 12; ++col) {
        // 等宽列构成三个等宽分区，随最大化/还原自动铺满，不留下右侧空白。
        m_results->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    }
    m_results->horizontalHeader()->setMinimumSectionSize(8);
    auto *an3Row = new QHBoxLayout;
    auto *an3Label = new QLabel(QStringLiteral("非标 AN3 原始空载电流(%)："), this);
    m_an3Raw = new QLineEdit(this);
    m_an3Raw->setObjectName(QStringLiteral("an3RawCurrent"));
    m_an3Raw->setStyleSheet(QStringLiteral("QLineEdit {background:#FF00FF; color:#24362B; border:1px solid #A8CDB3; padding:1px;} QLineEdit:disabled {background:white;}"));
    m_an3Raw->setPlaceholderText(QStringLiteral("例如 0.7"));
    m_an3Raw->setMaximumWidth(110);
    m_an3Status = new QLabel(this);
    m_an3Status->setObjectName(QStringLiteral("an3ConversionStatus"));
    m_an3Status->setWordWrap(true);
    an3Row->addWidget(an3Label); an3Row->addWidget(m_an3Raw); an3Row->addWidget(m_an3Status, 1);
    auto *supplement = new QPushButton(QStringLiteral("补充参数"), this);
    m_supplementButton = supplement;
    supplement->setObjectName(QStringLiteral("supplementParametersButton"));
    supplement->setToolTip(QStringLiteral("查看计算单辅助表及程序设计输入中未放入主表的参数和计算说明"));
    an3Row->addWidget(supplement);
    connect(supplement, &QPushButton::clicked, this, &SchemeCalculationSheet::showSupplement);
    outer->addLayout(an3Row);
    syncAn3Controls();
    connect(m_an3Raw, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_input->getParams().standardMode != TransformerParams::StandardMode::Custom || m_input->getParams().hasManualNoLoadCurrent()) return;
        bool ok = false;
        const double raw = text.trimmed().toDouble(&ok);
        m_an3Invalid = !ok || !std::isfinite(raw) || raw <= 0.34;
        if (m_an3Invalid) {
            m_an3Status->setText(QStringLiteral("请输入大于0.34的有效值；未更新O9，不能确认。"));
        } else if (m_input->setNonstandardAn3(raw)) {
            m_an3Status->setText(QStringLiteral("O9 = AN3 − 0.34 = %1%；原始值随方案保存。")
                .arg(QString::number(raw - 0.34, 'g', 12)));
            syncInputs();
        }
        markPending();
    });
    connect(m_input, &ParamTableWidget::stdValuesUpdated, this, [this](const QString &) {
        if (m_an3Invalid && m_input->getParams().standardMode == TransformerParams::StandardMode::Custom && !m_input->getParams().hasManualNoLoadCurrent()) {
            syncInputs();
            markPending();
            return;
        }
        m_an3Invalid = false;
        syncAn3Controls();
        syncInputs();
        markPending();
    });
    connect(m_input, &ParamTableWidget::nonstandardAn3StateChanged, this, [this]() {
        m_an3Invalid = false;
        syncAn3Controls();
    });
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
        connect(d, &QComboBox::currentIndexChanged, s, [s,d](int index) {
            s->setCurrentIndex(index);
            // A source handler may reject/revert a selection with signals blocked.
            // Always finish with the authoritative index, without a second edit.
            const QSignalBlocker block(d);
            d->setCurrentIndex(s->currentIndex());
        });
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
    if (m_results && source == m_results->viewport() && event->type() == QEvent::Resize && !m_columnFitPending) {
        m_columnFitPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_columnFitPending = false;
            fitColumnsToViewport();
        });
    }
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
    if (m_input->calculationConfig().coreShape != StructureConfig::LongRound)
        show(17,5,QStringLiteral("0 字")); // Original non-long-round F18 literal, not a thickness.
    show(4,5,StructureConfig::yokeShapeName(m_input->calculationConfig().yokeShape));
    show(5,2,m_coolingName);
    show(20,12,m_tankFormCombo&&m_tankFormCombo->currentIndex()>=0
        ?m_tankFormCombo->currentText():QStringLiteral("未选择油箱型式"));
    const auto *hvMaterial = qobject_cast<QComboBox *>(m_results->cellWidget(12,20));
    const auto *lvMaterial = qobject_cast<QComboBox *>(m_results->cellWidget(12,29));
    if (hvMaterial && lvMaterial) {
        const bool copperHv = hvMaterial->currentText() == QStringLiteral("铜");
        const bool copperLv = lvMaterial->currentText() == QStringLiteral("铜箔");
        show(5,5,copperHv == copperLv ? (copperHv ? QStringLiteral("全铜") : QStringLiteral("全铝"))
            : (copperHv ? QStringLiteral("高铜低铝") : QStringLiteral("高铝低铜")));
    }
    show(2,5,params.standardMode == TransformerParams::StandardMode::Custom ? QStringLiteral("非标")
        : params.standardMode == TransformerParams::StandardMode::BuiltIn ? QStringLiteral("国标") : QStringLiteral("待确认"));
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
        b.cell->setBackground(QColor(source->flags().testFlag(Qt::ItemIsEditable) ? "#FF00FF" : "#FFFFFF"));
        b.cell->setToolTip(source->toolTip().isEmpty() ? source->text() : source->toolTip());
        if(auto *edit=m_panelInputs.value(b.cell).data()) {
            if(edit->text()!=source->text()){const QSignalBlocker block(edit);edit->setText(source->text());}
            edit->setEnabled(source->flags().testFlag(Qt::ItemIsEditable));
        }
    }
    updateTankPanel();
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
                    if (displayName == QStringLiteral("心柱截面")) displayName = QStringLiteral("心柱设计截面 F14");
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
    const int tankForm=m_tankFormCombo?m_tankFormCombo->currentIndex():0;
    const int coilForm=m_coilFormCombo?m_coilFormCombo->currentData().toInt():m_previewBaseInput.hvCoilFormIdx;
    const QSignalBlocker blocker(m_results);
    m_bindings.clear(); m_editors.clear(); m_resultBindings.clear();
    m_panelInputs.clear();m_tankPanel=nullptr;m_tankFormCombo=nullptr;
    m_results->clearSpans(); m_results->setRowCount(0);
    m_results->setColumnCount(38); m_results->setRowCount(54);
    m_tankPanel=new TankDimensionsPanel(m_results);
    m_tankFormCombo=new QComboBox(m_tankPanel);
    m_tankFormCombo->setObjectName(QStringLiteral("sheetTankForm"));
    m_tankFormCombo->addItem(QStringLiteral("波纹油箱"),0);
    m_tankFormCombo->addItem(QStringLiteral("片散油箱"),1);
    m_tankFormCombo->setCurrentIndex(tankForm);
    m_tankFormCombo->setToolTip(QStringLiteral("原单两种油箱型式。当前完整算法仅支持波纹油箱；片散可选择查看，尚不能确认或用于寻优。"));
    m_tankPanel->installEditor(QStringLiteral("M21"),m_tankFormCombo);
    connect(m_tankFormCombo,&QComboBox::currentIndexChanged,this,[this]{syncInputs();markPending();});
    const auto layout = QJsonDocument::fromJson(calculationSheetTemplate).object();
    for (int col = 0; col < 38; ++col)
        m_results->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Fixed);
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
        if(a[0].toInt()<27&&a[0].toInt()+a[2].toInt()>20
            &&a[1].toInt()<19&&a[1].toInt()+a[3].toInt()>4)continue;
        m_results->setSpan(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt());
    }
    m_results->setSpan(20,4,7,15);
    m_results->setCellWidget(20,4,m_tankPanel);
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
            const bool inTank=row>=20&&row<=26&&col>=4&&col<=18;
            const QString address=QString(QChar('A'+col))+QString::number(row+1);
            if (auto *source = m_input->cellWidget(f.row,f.col)) {
                if (auto *editor = mirrorEditor(source)) {
                    if(inTank)m_tankPanel->installEditor(address,editor);
                    else m_results->setCellWidget(row,col,editor);
                }
            } else if(inTank) {
                auto *source=m_input->item(f.row,f.col);
                if(source&&source->flags().testFlag(Qt::ItemIsEditable)) {
                    auto *edit=new QLineEdit(source->text(),m_tankPanel);
                    edit->setObjectName(QStringLiteral("tankInput_")+address);
                    edit->setAlignment(Qt::AlignCenter);edit->setMinimumWidth(0);edit->setToolTip(f.tip);
                    m_panelInputs.insert(value,edit);m_tankPanel->installEditor(address,edit);
                    connect(edit,&QLineEdit::textEdited,this,[this,r=f.row,c=f.col](const QString &text){
                        if(auto *item=m_input->item(r,c))item->setText(text);
                    });
                }
            }
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
    inputAt(QStringLiteral("F3"),QStringLiteral("指标来源模式"));
    // Original dropdowns; unsupported structures are selectable for inspection,
    // but inputsReady rejects them before either preview calculation or confirmation.
    m_coreShapeCombo = new QComboBox(m_results);
    const auto selection = m_input->calculationConfig();
    for (const auto shape : {StructureConfig::Circle, StructureConfig::LongRound, StructureConfig::Ellipse})
        m_coreShapeCombo->addItem(StructureConfig::coreShapeName(shape)
            + (shape == StructureConfig::Ellipse ? QString() : QStringLiteral("（部分联动）")), int(shape));
    int shapeIndex = m_coreShapeCombo->findData(int(selection.coreShape));
    if (shapeIndex < 0) {
        m_coreShapeCombo->addItem(StructureConfig::coreShapeName(selection.coreShape)
            + QStringLiteral("（暂不支持计算）"), int(selection.coreShape));
        shapeIndex = m_coreShapeCombo->count() - 1;
    }
    m_coreShapeCombo->setCurrentIndex(shapeIndex);
    m_coreShapeCombo->setToolTip(QStringLiteral("椭圆形沿用完整计算；圆形/长圆形联动几何及已核实的绕组结果，整机重量、散热及温升待核对，不能确认或寻优。"));
    m_results->setCellWidget(3,5,m_coreShapeCombo);
    connect(m_coreShapeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const auto shape = static_cast<StructureConfig::CoreShape>(m_coreShapeCombo->currentData().toInt());
        m_coreShapeName = StructureConfig::coreShapeName(shape);
        m_input->setCoreSelection(shape, m_input->calculationConfig().yokeShape);
        syncInputs(); markPending();
    });
    m_yokeShapeCombo = new QComboBox(m_results);
    m_yokeShapeCombo->addItem(QStringLiteral("同心柱（暂不支持计算）"), int(StructureConfig::SameAsCore));
    m_yokeShapeCombo->addItem(QStringLiteral("T形轭"), int(StructureConfig::TYoke));
    int yokeIndex = m_yokeShapeCombo->findData(int(selection.yokeShape));
    if (yokeIndex < 0) {
        m_yokeShapeCombo->addItem(StructureConfig::yokeShapeName(selection.yokeShape)
            + QStringLiteral("（暂不支持计算）"), int(selection.yokeShape));
        yokeIndex = m_yokeShapeCombo->count() - 1;
    }
    m_yokeShapeCombo->setCurrentIndex(yokeIndex);
    m_yokeShapeCombo->setToolTip(QStringLiteral("当前按T形轭计算；同心柱尚无独立对应算法，禁止计算或确认。"));
    m_results->setCellWidget(4,5,m_yokeShapeCombo);
    connect(m_yokeShapeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_input->setCoreSelection(m_input->calculationConfig().coreShape,
            static_cast<StructureConfig::YokeShape>(m_yokeShapeCombo->currentData().toInt()));
        syncInputs(); markPending();
    });
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
    cell(7,34,QStringLiteral("参考磁密"),QStringLiteral("原单 AM8：仅用于推荐匝数"));
    inputAt(QStringLiteral("AJ8"),QStringLiteral("低压参考磁密(T)"));
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
    // Original workbook's high-voltage coil selector overlays AC9.
    m_coilFormCombo=new QComboBox(m_results);
    m_coilFormCombo->setObjectName(QStringLiteral("sheetCoilForm"));
    m_coilFormCombo->addItem(QStringLiteral("圆筒式"),1);
    m_coilFormCombo->addItem(QStringLiteral("分段式"),2);
    int coilIndex=m_coilFormCombo->findData(coilForm);
    if(coilIndex<0) {
        m_coilFormCombo->addItem(QStringLiteral("未知"),coilForm);
        coilIndex=m_coilFormCombo->count()-1;
    }
    m_coilFormCombo->setCurrentIndex(coilIndex);
    m_coilFormCombo->setToolTip(HvCoilFormNotes::processNote()+QStringLiteral("\n分段式整机计算仅覆盖五组油道均启用的情况。"));
    m_results->setCellWidget(8,28,m_coilFormCombo);
    connect(m_coilFormCombo,&QComboBox::currentIndexChanged,this,[this] {
        const int form = m_coilFormCombo->currentData().toInt();
        if (form == 1 || form == 2) {
            auto config = m_input->calculationConfig();
            config.hvCoilStructure = form == 2 ? StructureConfig::TwoSegCylinder : StructureConfig::MultiLayerCylinder;
            m_input->setCalculationConfig(config);
        }
        markPending();
    });
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
    // Keep the worksheet itself at its original 54 rows. Inputs not mapped to
    // the main sheet remain editable in the same source table via the dialog.
    m_supplementRows.clear();
    m_supplementKeys.clear();
    for (const auto &group : {advanced, checks}) {
        for (const auto &f : group) {
            if (f.row >= 0) {
                auto *source = m_input->item(f.row, f.col);
                auto *widget = m_input->cellWidget(f.row, f.col);
                if ((source && source->flags().testFlag(Qt::ItemIsEditable)) ||
                    (widget && (qobject_cast<QComboBox *>(widget) || qobject_cast<QAbstractSpinBox *>(widget)
                     || qobject_cast<QLineEdit *>(widget) || widget->findChild<QComboBox *>()
                     || widget->findChild<QAbstractSpinBox *>() || widget->findChild<QLineEdit *>())))
                    m_supplementRows.insert(f.row);
            } else if (!f.key.isEmpty() && !m_supplementKeys.contains(f.key)) {
                m_supplementKeys.append(f.key);
            }
        }
    }
    for (int r=0; r<m_results->rowCount(); ++r)
        for (int c=0; c<38; ++c)
            if (!m_results->item(r,c)) cell(r,c,QString());
    for (int r : {6,7,8,9,10,11,12,13,14,15,22,24,32,33,35,38,42,43,44,46,47,48,49,50,51,52,53})
        m_results->setRowHeight(r,36);
    int tankHeight=0;for(int r=20;r<=26;++r)tankHeight+=m_results->rowHeight(r);
    const int extra=std::max(0,m_tankPanel->sizeHint().height()+8-tankHeight);
    for(int r=20;r<=26;++r)m_results->setRowHeight(r,m_results->rowHeight(r)+(extra+6)/7);
    syncInputs(); mergeFieldSpace(); fitColumnsToViewport(); fitTextRows();
    m_results->verticalScrollBar()->setValue(scroll);
}

void SchemeCalculationSheet::updateTankPanel(const QMap<QString, QPair<double, int>> *overrides)
{
    if(!m_tankPanel)return;
    QMap<QString,QString> values;
    const QStringList addresses=QStringLiteral("M21 Q22 F23 H23 J23 S23 F24 H24 J24 N24 O24 F25 H25 J25 Q25 H26 J26 L26 Q26 E27 F27 J27").split(' ');
    for(const auto &address:addresses){
        int col=0,index=0;while(index<address.size()&&address[index].isLetter())col=col*26+address[index++].unicode()-'A'+1;
        if(auto *item=m_results->item(address.mid(index).toInt()-1,col-1))values.insert(address,item->text());
    }
    if (overrides) {
        for (const auto &address : addresses) {
            const auto it = overrides->constFind(address);
            if (it != overrides->cend()) values.insert(address, QString::number(it->first, 'f', it->second));
        }
    }
    m_tankPanel->setValues(values);
}

void SchemeCalculationSheet::mergeFieldSpace()
{
    // Excel lets a field visually overflow into empty neighbouring cells. A Qt
    // table does not. Give that field the unused space without covering any
    // independent value/editor, vertical merge, or the next calculation block.
    constexpr int rows=54,columns=38;
    QSet<int> leftBorders;
    const auto sourceLayout=QJsonDocument::fromJson(calculationSheetTemplate).object();
    for(const auto &entry:sourceLayout.value(QStringLiteral("leftBorders")).toArray()) {
        const auto boundary=entry.toArray();
        leftBorders.insert(boundary[0].toInt()*columns+boundary[1].toInt());
    }
    QSet<QTableWidgetItem *> boundCells;
    for(const auto &binding:m_bindings)boundCells.insert(binding.cell);
    for(auto *item:m_resultBindings)boundCells.insert(item);
    QVector<int> owners(rows*columns,-1);
    for(int r=0;r<rows;++r)for(int c=0;c<columns;++c){
        if(owners[r*columns+c]>=0)continue;
        const int height=m_results->rowSpan(r,c),width=m_results->columnSpan(r,c);
        for(int y=r;y<r+height&&y<rows;++y)for(int x=c;x<c+width&&x<columns;++x)
            owners[y*columns+x]=r*columns+c;
    }
    for(int r=2;r<rows;++r)for(int c=0;c<columns;++c){
        // Keep the source workbook grid, including intentional blank cells,
        // below row 26 and in the left weight/centre-distance block above it.
        if(c==19||r>=26||(r>=22&&c<4))continue;
        const int owner=r*columns+c;
        if(owners[owner]!=owner||m_results->rowSpan(r,c)!=1)continue;
        auto *item=m_results->item(r,c);
        if((!item||item->text().trimmed().isEmpty())&&!m_results->cellWidget(r,c)&&!boundCells.contains(item))continue;
        const int originalWidth=m_results->columnSpan(r,c);
        int end=c+originalWidth;
        const int boundary=c<19?19:c<29?29:columns;
        while(end<boundary){
            // Explicit source borders stop overflow even when the neighbour is empty.
            if(c<19&&leftBorders.contains(r*columns+end-1))break;
            const int next=r*columns+end;
            auto *neighbour=m_results->item(r,end);
            if(owners[next]!=next||m_results->rowSpan(r,end)!=1||m_results->columnSpan(r,end)!=1
                ||m_results->cellWidget(r,end)||boundCells.contains(neighbour)
                ||(neighbour&&!neighbour->text().trimmed().isEmpty()))break;
            owners[next]=owner;++end;
        }
        if(end>c+originalWidth)m_results->setSpan(r,c,1,end-c);
    }
    // Left A-column blank blocks have no internal source border.
    // Preserve independent numeric values in column B alongside these blocks.
    for(const auto range: {qMakePair(32,3),qMakePair(41,3),qMakePair(45,2)}) {
        auto *block=new WorksheetLabelBlock(m_results,range.first,range.second,0);
        m_results->setSpan(range.first,0,range.second,1);
        m_results->setCellWidget(range.first,0,block);
    }
    for(const auto region: {std::array<int,3>{41,3,1},std::array<int,3>{32,3,2},
                            std::array<int,3>{32,3,3},std::array<int,3>{48,2,3}}) {
        auto *block=new WorksheetLabelBlock(m_results,region[0],region[1],region[2]);
        m_results->setSpan(region[0],region[2],region[1],1);
        m_results->setCellWidget(region[0],region[2],block);
    }
    // Actual source borders: T9:T12, T13:T14, T22:T43,
    // T44:T45 and T46:T54. Faint Excel gridlines are not cell boundaries.
    m_results->setSpan(9,19,1,1); // Remove original T10:T11 before joining its block.
    for(const auto range: {qMakePair(8,4),qMakePair(12,2),qMakePair(21,22),
                            qMakePair(43,2),qMakePair(45,9)}) {
        auto *block=new WorksheetLabelBlock(m_results,range.first,range.second);
        m_results->setSpan(range.first,19,range.second,1);
        m_results->setCellWidget(range.first,19,block);
    }
}

void SchemeCalculationSheet::fitColumnsToViewport()
{
    if (!m_results || m_results->columnCount()!=38 || m_results->rowCount()<54) return;
    const int available = m_results->viewport()->width() - 1;
    if (available < 38 * 8) return;
    const auto templateObject=QJsonDocument::fromJson(calculationSheetTemplate).object();
    const auto original=templateObject.value(QStringLiteral("widths")).toArray();
    QVector<int> minimum(38, 18);
    const QFontMetrics metrics(m_results->font());
    const int numericWidth=metrics.horizontalAdvance(QStringLiteral("000.00"))+4;
    for (int r=0;r<54;++r) for(int c=0;c<38;++c) {
        auto *item=m_results->item(r,c);
        if (!item || m_results->columnSpan(r,c)!=1 || m_results->rowSpan(r,c)!=1) continue;
        bool isNumber=false; item->text().trimmed().toDouble(&isNumber);
        if (isNumber || item->flags().testFlag(Qt::ItemIsEditable))
            minimum[c]=std::max(minimum[c],numericWidth);
        else {
            const QString name=item->text().trimmed();
            if (!name.isEmpty() && name.size()<=3 && !name.contains(QLatin1Char('\n')))
                minimum[c]=std::max(minimum[c],std::min(90,metrics.horizontalAdvance(name)+10));
        }
    }
    minimum[0]=std::max(minimum[0],metrics.horizontalAdvance(QStringLiteral("相间距"))+26);
    minimum[4]=std::max(minimum[4],metrics.horizontalAdvance(QStringLiteral("标准"))+22);
    minimum[8]=std::max(minimum[8],metrics.horizontalAdvance(QStringLiteral("000.00"))+12);
    minimum[16]=std::max(minimum[16],metrics.horizontalAdvance(QStringLiteral("大圆心角"))+10);
    minimum[19]=std::max(minimum[19],metrics.horizontalAdvance(QStringLiteral("75℃电阻(Ω)"))+12);
    minimum[28]=std::max(minimum[28],metrics.horizontalAdvance(QStringLiteral("圆筒式"))+25);
    // In a narrow window keep the full sheet visible; longer text can wrap vertically.
    int minSum=0; for(int v:minimum)minSum+=v;
    if (minSum>available) {
        const int fixed=38*8;
        const double ratio=double(available-fixed)/double(minSum-fixed);
        for(int &v:minimum)v=8+int((v-8)*ratio);
        minSum=0;for(int v:minimum)minSum+=v;
    }
    double totalWeight=0.0;
    for(const auto &v:original)totalWeight+=std::max(0.1,v.toDouble());
    const int extra=std::max(0,available-minSum);
    int used=0;
    QVector<int> widths(38);
    for(int c=0;c<38;++c) {
        widths[c]=minimum[c]+int(extra*std::max(0.1,original[c].toDouble())/totalWeight);
        used+=widths[c];
    }
    for(int i=0;used<available;++i,++used)++widths[i%38];
    for(int c=0;c<38;++c)if(m_results->columnWidth(c)!=widths[c])m_results->setColumnWidth(c,widths[c]);
    // Recalculate heights from the baseline on every resize so a temporarily
    // narrow window does not leave permanently oversized rows after widening.
    for(int r=0;r<54;++r)m_results->setRowHeight(r,28);
    for(int r : {6,7,8,9,10,11,12,13,14,15,22,24,32,33,35,38,42,43,44,46,47,48,49,50,51,52,53})
        m_results->setRowHeight(r,36);
    if(m_tankPanel) {
        int height=0;for(int r=20;r<=26;++r)height+=m_results->rowHeight(r);
        const int extra=std::max(0,m_tankPanel->sizeHint().height()+8-height);
        for(int r=20;r<=26;++r)m_results->setRowHeight(r,m_results->rowHeight(r)+(extra+6)/7);
    }
    fitTextRows();
}

void SchemeCalculationSheet::fitTextRows()
{
    // Keep editor instances and current row positions; only grow a row when a
    // current value/description needs more space. Refreshes never rebuild it.
    const int rows=m_results->rowCount(),columns=m_results->columnCount();
    QVector<bool> covered(rows*columns,false);
    for(int r=0;r<rows;++r)for(int c=0;c<columns;++c){
        if(covered[r*columns+c])continue;
        const int height=m_results->rowSpan(r,c),width=m_results->columnSpan(r,c);
        int pixels=0,available=0;
        for(int x=c;x<c+width;++x)pixels+=m_results->columnWidth(x);
        for(int y=r;y<r+height;++y){available+=m_results->rowHeight(y);for(int x=c;x<c+width;++x)covered[y*columns+x]=true;}
        int needed=0;
        if(auto *widget=m_results->cellWidget(r,c))needed=widget->sizeHint().height()+4;
        else if(auto *item=m_results->item(r,c)){
            const QFontMetrics metrics(item->font());
            needed=metrics.boundingRect(QRect(0,0,std::max(1,pixels-12),100000),
                Qt::TextWordWrap|Qt::TextWrapAnywhere,item->text()).height()+10;
        }
        if(needed>available)m_results->setRowHeight(r+height-1,m_results->rowHeight(r+height-1)+needed-available);
    }
}

void SchemeCalculationSheet::markPending()
{
    if (!refreshPartialPreview()) {
        if (m_coilFormCombo && m_snapshot.valid
            && m_coilFormCombo->currentData().toInt() != m_snapshot.winding.hvCoilFormIdx) {
            const QSignalBlocker blocker(m_results);
            QSet<QTableWidgetItem *> inputs;
            for (const auto &binding : m_bindings) inputs.insert(binding.cell);
            for (auto *cell : m_resultBindings) {
                if (!inputs.contains(cell)) {
                    cell->setText(QStringLiteral("待重新计算"));
                    cell->setToolTip(QStringLiteral("线圈型式已变更，旧方案结果不可沿用。"));
                }
            }
        }
        if (m_partialActive) {
            const QSignalBlocker blocker(m_results);
            for (auto *cell : m_resultBindings) {
                cell->setText(QStringLiteral("待重新计算"));
                cell->setToolTip(QStringLiteral("已切回完整计算分支，等待本次重算；不沿用部分联动结果。"));
            }
            m_partialActive = false;
        }
        m_partialStatus.clear();
        m_resultStatus->setText(QStringLiteral("品红色：可输入　白色：只读　输入已修改，正在自动更新；旧结果暂未刷新"));
        updateTankPanel();
        if(m_tankPanel)m_tankPanel->setCalculationStatus(QStringLiteral("参数已修改，尺寸待重算；旧值仅供查看。"));
    }
    emit inputEdited();
}

void SchemeCalculationSheet::refreshInputValues()
{
    syncInputs();
    refreshPartialPreview();
    fitTextRows();
}

bool SchemeCalculationSheet::refreshPartialPreview()
{
    const QString tankError=tankSelectionError();
    const bool segmentSelected = m_coilFormCombo && m_coilFormCombo->currentData().toInt() == 2;
    const bool segmentPreview = segmentSelected && !coilSelectionError().isEmpty();
    const QString selectionError=tankError.isEmpty() && !segmentPreview ? coilSelectionError() : tankError;
    if(!selectionError.isEmpty()){
        m_partialActive=true;m_partialStatus=selectionError;
        const QSignalBlocker blocker(m_results);
        for(auto *cell:m_resultBindings){
            cell->setText(QStringLiteral("待核对"));cell->setForeground(QColor("#93642E"));
            cell->setToolTip(selectionError+QStringLiteral("\n不沿用旧结构结果，原方案未修改。"));
        }
        // Shape-specific F17/F18 are updated outside m_resultBindings.
        for(int row:{16,17})if(auto *cell=m_results->item(row,5)){
            cell->setText(QStringLiteral("待核对"));cell->setForeground(QColor("#93642E"));cell->setToolTip(selectionError);
        }
        m_resultStatus->setText(selectionError);updateTankPanel();
        if(m_tankPanel)m_tankPanel->setCalculationStatus(selectionError);
        fitTextRows();return true;
    }
    const auto config = m_input->calculationConfig();
    if (!segmentPreview && config.coreShape == StructureConfig::Ellipse && config.yokeShape == StructureConfig::TYoke) return false;
    m_partialActive = true;
    CalcInput current = m_previewBaseInput;
    ShapeBranchPreview partial;
    if (editorsReady(partial.error) && m_input->collectShapePreviewInput(current, partial.error, segmentPreview)) {
        if (segmentPreview && captureCoilForm(current, partial.error))
            partial = ElectromagneticEngine::previewSegmentWinding(current);
        else if (!segmentPreview)
            partial = ElectromagneticEngine::previewShapeBranch(current);
    }
    const QString scope = segmentPreview
        ? QStringLiteral("分段式部分联动：绕组、损耗、阻抗和温升仅供核对；整机校核尚未完成，不能确认或寻优。")
        : QStringLiteral("部分联动，未完成整机校核：片宽/几何半叠厚及已核实的绕组结果可查看；截面工艺、铁芯重量、空载/总损耗、阻抗、散热、温升和材料总价待核对，不能确认或寻优。");
    const QSignalBlocker blocker(m_results);
    const auto show = [&](QTableWidgetItem *cell, const QString &address) {
        if (!cell) return;
        const auto number = partial.values.constFind(address);
        QString value = QStringLiteral("待核对");
        if (partial.texts.contains(address)) value = partial.texts.value(address);
        else if (number != partial.values.cend()) value = QString::number(number->first, 'f', number->second);
        cell->setText(value);
        cell->setTextAlignment((number != partial.values.cend() ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        cell->setForeground(QColor(number != partial.values.cend() ? "#185C37" : "#93642E"));
        cell->setToolTip(address + QLatin1Char('\n') + scope + QLatin1Char('\n')
            + partial.note + QLatin1Char('\n') + partial.error);
    };
    const auto &owners = originalResultAddresses();
    for (auto it = m_resultBindings.cbegin(); it != m_resultBindings.cend(); ++it) {
        const QString address = it.key().startsWith(QStringLiteral("原单\n"))
            ? it.key().section(QLatin1Char('\n'), 1) : owners.value(it.key());
        show(it.value(), address);
    }
    // F17/F18 are shape-specific header cells, not extra inputs or new rows.
    show(m_results->item(16, 5), QStringLiteral("F17"));
    show(m_results->item(17, 5), QStringLiteral("F18"));
    m_partialStatus = scope + QLatin1Char('\n') + (segmentPreview ? partial.note + QLatin1Char('\n') : QString())
        + (partial.windingAvailable ? (segmentPreview ? QStringLiteral("本次分段式绕组联动已更新。") : QStringLiteral("本次几何与绕组联动已更新。"))
            : partial.geometryAvailable ? QStringLiteral("本次仅几何联动已更新。") : QStringLiteral("本次部分联动未生成有效数值。"))
        + (partial.error.isEmpty() ? QString() : QLatin1Char('\n') + partial.error);
    m_resultStatus->setText(m_partialStatus);
    updateTankPanel(segmentPreview ? &partial.values : nullptr);
    if(m_tankPanel) {
        const bool dimensionsReady = segmentPreview && partial.values.contains(QStringLiteral("H26"))
            && partial.values.contains(QStringLiteral("F25")) && partial.values.contains(QStringLiteral("J27"));
        m_tankPanel->setCalculationStatus(dimensionsReady
            ? QStringLiteral("分段式尺寸预览，仅供核对，非整机确认。")
            : QStringLiteral("当前结构仅部分联动，油箱尺寸待核对。"));
    }
    fitTextRows();
    return true;
}

bool SchemeCalculationSheet::inputsReady(QString &error) const
{
    error=tankSelectionError();if(!error.isEmpty())return false;
    error=coilSelectionError();if(!error.isEmpty())return false;
    CalcInput selection;
    m_input->saveToInput(selection);
    error = selection.coreSelectionError();
    if (!error.isEmpty()) return false;
    return editorsReady(error);
}

bool SchemeCalculationSheet::captureCoilForm(CalcInput &input, QString &error) const
{
    const int form = m_coilFormCombo ? m_coilFormCombo->currentData().toInt() : 0;
    if (form != 1 && form != 2) {
        error = QStringLiteral("请选择有效的高压线圈型式。");
        return false;
    }
    input.hvCoilFormIdx = form;
    error.clear();
    return true;
}

QString SchemeCalculationSheet::coilSelectionError() const
{
    if (!m_coilFormCombo) return QStringLiteral("请选择高压线圈型式。");
    const int form = m_coilFormCombo->currentData().toInt();
    if (form != 1 && form != 2) return QStringLiteral("高压线圈型式无效。");
    if (form == 2) {
        CalcInput candidate = m_previewBaseInput;
        m_input->saveToInput(candidate);
        candidate.hvCoilFormIdx = 2;
        return candidate.segmentedCoilScopeError();
    }
    return {};
}

QString SchemeCalculationSheet::tankSelectionError() const
{
    if(!m_tankFormCombo||m_tankFormCombo->currentIndex()<0)
        return QStringLiteral("请选择有效的油箱型式；尺寸待核对。" );
    if(m_tankFormCombo->currentData().toInt()!=0)
        return QStringLiteral("片散油箱完整算法尚未接入，尺寸及性能待核对；不能确认或用于寻优。" );
    return {};
}

void SchemeCalculationSheet::showSupplement()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("supplementParametersDialog"));
    dialog.setWindowTitle(QStringLiteral("补充参数"));
    dialog.resize(1050, 680);
    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(QStringLiteral("包含原计算单辅助表参数及设计输入中的补充设置。"), &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *tabs = new QTabWidget(&dialog);
    layout->addWidget(tabs, 1);
    QVector<bool> hidden;
    hidden.reserve(m_input->rowCount());
    for (int row = 0; row < m_input->rowCount(); ++row) hidden.append(m_input->isRowHidden(row));
    // Include headings only when they precede at least one retained input.
    QSet<int> visible = m_supplementRows;
    int heading = -1;
    for (int row = 0; row < m_input->rowCount(); ++row) {
        const auto *label = m_input->item(row, 1);
        if (label && label->font().bold()) heading = row;
        if (m_supplementRows.contains(row) && heading >= 0) visible.insert(heading);
    }
    m_input->setParent(tabs);
    for (int row = 0; row < m_input->rowCount(); ++row)
        m_input->setRowHidden(row, !visible.contains(row) || hidden.at(row));
    tabs->addTab(m_input, QStringLiteral("参数"));
    m_input->show();
    auto *details = new QTextBrowser(tabs);
    details->setObjectName(QStringLiteral("supplementDetails"));
    m_supplementDetails = details;
    tabs->addTab(details, QStringLiteral("计算说明"));
    refreshSupplementDetails();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("完成"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog.exec();
    tabs->removeTab(tabs->indexOf(m_input));
    m_input->hide();
    m_input->setParent(this);
    for (int row = 0; row < m_input->rowCount() && row < hidden.size(); ++row)
        m_input->setRowHidden(row, hidden.at(row));
    m_supplementDetails = nullptr;
}

void SchemeCalculationSheet::refreshSupplementDetails()
{
    if (!m_supplementDetails) return;
    QString html;
    for (const auto &group : sheetGroups(m_snapshot)) {
        QString content;
        for (const auto &values : group.second) {
            const QString key = group.first + QLatin1Char('\n') + values.value(0);
            if (!m_supplementKeys.contains(key)) continue;
            const QString value = values.value(1) + (values.value(2).isEmpty() ? QString() : QLatin1Char(' ') + values.value(2));
            content += QStringLiteral("<p><b>%1：</b>%2<br/><small>%3</small></p>")
                .arg(values.value(0).toHtmlEscaped(), value.toHtmlEscaped(), values.value(3).toHtmlEscaped());
        }
        if (!content.isEmpty()) html += QStringLiteral("<h3>%1</h3>%2").arg(group.first.toHtmlEscaped(), content);
    }
    if (m_snapshot.valid && m_snapshot.sheetDetailsSnapshotAvailable
            && m_snapshot.calculationSheetDetails.contains(QStringLiteral("CoreSheetCount1"))) {
        QString coreHtml=QStringLiteral("<h3>铁芯逐级片数与实际叠厚</h3><p>当前已计算方案的快照；修改参数后需重新计算。统一采用SB20固定默认规则：片宽由当前尺寸查表基数加固定默认偏移求得，片数按固定默认增减量计算；此处仅显示计算快照。第17—20级备用，尚未参与计算。</p><table border='1' cellspacing='0' cellpadding='5'><tr><th>级次</th><th>查表基数(mm)</th><th>固定默认片宽偏移(mm)</th><th>片宽(mm)</th><th>基础片数</th><th>固定默认增减(片)</th><th>实际片数</th><th>实际总叠厚(mm)</th></tr>");
        for(int i=1;i<=16;++i) {
            if(!m_snapshot.calculationSheetDetails.contains(QStringLiteral("CoreSheetCount%1").arg(i))) continue;
            const auto value=[this,i](const char *prefix){return m_snapshot.calculationSheetDetails.value(QString::fromLatin1(prefix)+QString::number(i)).first;};
            const auto widthText=[this,i](const char *prefix){
                const auto it=m_snapshot.calculationSheetDetails.constFind(QString::fromLatin1(prefix)+QString::number(i));
                return it==m_snapshot.calculationSheetDetails.cend()?QStringLiteral("—"):QString::number(it.value().first,'g',15);
            };
            coreHtml+=QStringLiteral("<tr><td>%1（L%2）</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td><td>%8</td><td>%9</td></tr>")
                .arg(i).arg(i+4).arg(widthText("CoreWidthBase")).arg(widthText("CoreWidthOffset"))
                .arg(widthText("CoreFinalWidth")).arg(value("CoreSheetBase"),0,'f',0).arg(value("CoreSheetAdjust"),0,'f',0)
                .arg(value("CoreSheetCount"),0,'f',0).arg(value("CoreActualStack"),0,'f',4);
        }
        for(int i=17;i<=20;++i) {
            const auto key=QStringLiteral("CoreReservedSheetAdjust%1").arg(i);
            if(!m_snapshot.calculationSheetDetails.contains(key))continue;
            coreHtml+=QStringLiteral("<tr><td>%1（备用）</td><td>—</td><td>%2</td><td>未启用</td><td>—</td><td>%3</td><td>未启用</td><td>—</td></tr>")
                .arg(i).arg(m_snapshot.calculationSheetDetails.value(QStringLiteral("CoreReservedWidthOffset%1").arg(i)).first,0,'g',15)
                .arg(m_snapshot.calculationSheetDetails.value(key).first,0,'f',0);
        }
        coreHtml+=QStringLiteral("</table>");
        if(m_snapshot.calculationSheetDetails.contains(QStringLiteral("CoreWeightCenter"))){
            const auto value=[this](const char *key){return m_snapshot.calculationSheetDetails.value(QString::fromLatin1(key)).first;};
            coreHtml+=QStringLiteral("<h3>铁芯分项重量核对</h3><p>窗高：%1 mm；中心距：%2 mm；固定默认中心距附加量：%3 mm；固定默认层绝缘计算系数：%4（SB20规则）。</p><p>心柱合重：%5 kg；铁轭重：%6 kg；铁芯总重（取整）：%7 kg。</p><table border='1' cellspacing='0' cellpadding='5'><tr><th>级次</th><th>轭片宽(mm)</th><th>轭阶梯(mm)</th><th>外柱重(kg)</th><th>中柱重(kg)</th><th>铁轭重(kg)</th></tr>")
                .arg(value("CoreWeightWindow"),0,'g',15).arg(value("CoreWeightCenter"),0,'g',15)
                .arg(value("CoreCenterAllowance"),0,'g',15).arg(value("HVLayerCalcFactor"),0,'g',15)
                .arg(m_snapshot.core.coreLegsWeight_kg,0,'f',2).arg(m_snapshot.core.yokesWeight_kg,0,'f',2).arg(m_snapshot.core.coreWeight_kg,0,'f',0);
            for(int i=1;i<=16;++i){
                if(!m_snapshot.calculationSheetDetails.contains(QStringLiteral("CoreYokeWeight%1").arg(i)))continue;
                const auto stageValue=[this,i](const char *key){return m_snapshot.calculationSheetDetails.value(QString::fromLatin1(key)+QString::number(i)).first;};
                coreHtml+=QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>").arg(i)
                    .arg(stageValue("CoreYokeWidth"),0,'g',15).arg(stageValue("CoreYokeStep"),0,'g',15)
                    .arg(stageValue("CoreOuterLegWeight"),0,'f',2).arg(stageValue("CoreMiddleLegWeight"),0,'f',2)
                    .arg(stageValue("CoreYokeWeight"),0,'f',2);
            }
            coreHtml+=QStringLiteral("</table>");
        }
        html=coreHtml+html;
    }
    m_supplementDetails->setHtml(html);
}

void SchemeCalculationSheet::syncAn3Controls()
{
    if (!m_an3Raw || !m_an3Status || !m_input) return;
    const auto params = m_input->getParams();
    const bool custom = params.standardMode == TransformerParams::StandardMode::Custom;
    const QSignalBlocker blocker(m_an3Raw);
    const bool manual=params.hasManualNoLoadCurrent();
    m_an3Raw->setEnabled(custom&&!manual);
    m_an3Raw->setText(params.hasNonstandardAn3 ? QString::number(params.nonstandardAn3_pct, 'g', 15) : QString());
    if(manual){m_an3Invalid=false;m_an3Status->setText(QStringLiteral("O9手工固定为%1%；切换国标/非标不覆盖，AN3不参与换算。").arg(PerformanceCriteria::valueText(params.noLoadCurrentStd_pct)));}
    else if (custom && params.hasNonstandardAn3)
        m_an3Status->setText(QStringLiteral("O9 = AN3 − 0.34 = %1%；原始值随方案保存。")
            .arg(QString::number(params.noLoadCurrentStd_pct, 'g', 12)));
    else if (custom)
        m_an3Status->setText(QStringLiteral("自动来源尚未输入AN3；请填入有效值后确认，或选择手工固定。"));
    else
        m_an3Status->setText(QStringLiteral("%1；AN3仅在非标使用。").arg(m_input->standardStatus()));
}

bool SchemeCalculationSheet::editorsReady(QString &error) const
{
    const auto params=m_input->getParams();
    if (params.standardMode == TransformerParams::StandardMode::Custom && !params.hasManualNoLoadCurrent()
            && (m_an3Invalid || !params.hasNonstandardAn3)) {
        error = QStringLiteral("AN3尚未输入有效值，不能沿用旧标准值；请输入大于0.34的数值。");
        return false;
    }
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
    if (refreshPartialPreview()) return;
    m_resultStatus->setText(QStringLiteral("自动计算未更新（原结果仅供查看）：") + error);
    updateTankPanel();
    if(m_tankPanel)m_tankPanel->setCalculationStatus(QStringLiteral("自动计算未更新，旧尺寸仅供查看：")+error);
}

void SchemeCalculationSheet::refreshResult(const CalcResult &result)
{
    if (result.valid && m_coilFormCombo && m_coilFormCombo->currentData().toInt() != result.winding.hvCoilFormIdx) {
        markPending(); return;
    }
    if (refreshPartialPreview()) return;
    m_partialActive = false;
    m_partialStatus.clear();
    m_snapshot = result;
    refreshSupplementDetails();
    const QSignalBlocker blocker(m_results);
    for (const auto &group : sheetGroups(result)) {
        for (const auto &row : group.second) {
            const auto cells = m_resultBindings.values(group.first + QLatin1Char('\n') + row.value(0));
            for (auto *cell : cells) {
            cell->setForeground(QColor("#24362B"));
            const QString value = row.value(1) + (row.value(2).isEmpty() ? QString() : QLatin1Char(' ') + row.value(2));
            cell->setText(value); cell->setToolTip(row.value(0) + QStringLiteral("\n计算值（只读）：") + value
                + (row.value(3).isEmpty() ? QString() : QLatin1Char('\n') + row.value(3)));
            bool numeric = false; row.value(1).toDouble(&numeric);
            cell->setTextAlignment((numeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
            }
        }
    }
    m_resultStatus->setText(QStringLiteral("原计算单布局　品红色：可输入　白色：只读　已按当前参数自动计算；— 表示该字段未接入，不能用于判定"));
    updateTankPanel();
    if(m_tankPanel)m_tankPanel->setCalculationStatus(result.valid?QString():QStringLiteral("尺寸不可用，待重新计算。"));
    fitTextRows();
}

void SchemeCalculationSheet::loadResult(const CalcResult &result)
{
    m_snapshot = result;
    rebuildSheet();
    if (result.valid && m_coilFormCombo && m_coilFormCombo->currentData().toInt() != result.winding.hvCoilFormIdx) {
        markPending(); return;
    }
    if (refreshPartialPreview()) return;
    m_partialActive = false;
    m_partialStatus.clear();
    m_resultStatus->setText(result.valid ? QStringLiteral("原计算单布局　品红色：可输入　白色：只读　—：未接入当前方案数据（悬停查看）；其他参数可在顶部查看")
        : QStringLiteral("计算结果不可用，修改参数后自动计算；品红色单元格可输入"));
    updateTankPanel();
    if(m_tankPanel)m_tankPanel->setCalculationStatus(result.valid?QString():QStringLiteral("尺寸不可用，待重新计算。"));
}

namespace {
QString outputWidgetText(QWidget *widget)
{
    if (auto *combo = qobject_cast<QComboBox *>(widget)) return combo->currentText();
    if (auto *spin = qobject_cast<QDoubleSpinBox *>(widget)) return spin->text();
    if (auto *spin = qobject_cast<QSpinBox *>(widget)) return spin->text();
    if (auto *line = qobject_cast<QLineEdit *>(widget)) return line->text();
    if (auto *label = qobject_cast<QLabel *>(widget)) return label->text();
    QStringList parts;
    if (auto *layout = widget->layout()) {
        for (int i = 0; i < layout->count(); ++i)
            if (auto *child = layout->itemAt(i)->widget()) parts << outputWidgetText(child);
    }
    return parts.join(QStringLiteral(" "));
}
}

void SchemeCalculationSheet::setSupplementParametersVisible(bool visible)
{
    m_supplementButton->setVisible(visible);
}

void SchemeCalculationSheet::setOutputMode()
{
    // Keep only the worksheet. AN3 and supplement controls belong to editing.
    for (auto *child : findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        if (child != m_results) child->hide();
    const QSignalBlocker blocker(m_results);
    QFont outputFont(QStringLiteral("Microsoft YaHei"));
    outputFont.setPixelSize(13);
    m_results->setFont(outputFont);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setSelectionMode(QAbstractItemView::NoSelection);
    m_results->verticalHeader()->hide();
    m_results->setStyleSheet(QStringLiteral(
        "QTableWidget {background:white; color:black; font-size:13px; gridline-color:#777; border:1px solid black;}"
        "QLabel {background:white; color:black;}"));
    for (int r = 0; r < m_results->rowCount(); ++r) {
        for (int c = 0; c < m_results->columnCount(); ++c) {
            auto *cell = m_results->item(r,c);
            if (auto *widget = m_results->cellWidget(r,c)) {
                if (widget != m_tankPanel && !widget->property("worksheetLabelBlock").toBool()) {
                    if (cell) cell->setText(outputWidgetText(widget));
                    widget->hide();
                    m_results->removeCellWidget(r,c);
                }
            }
            if (cell) {
                cell->setFlags(Qt::ItemIsEnabled);
                cell->setBackground(Qt::white);
                cell->setForeground(Qt::black);
            }
        }
    }
    // Preserve dimensions and schematic while replacing the panel's editors.
    if (m_tankPanel) {
        for (auto *widget : m_tankPanel->findChildren<QWidget *>()) {
            if (!qobject_cast<QComboBox *>(widget) && !qobject_cast<QAbstractSpinBox *>(widget)
                && !qobject_cast<QLineEdit *>(widget)) continue;
            // Spin boxes own line edits; replace only top-level input controls.
            if (qobject_cast<QAbstractSpinBox *>(widget->parentWidget())) continue;
            auto *parent = widget->parentWidget();
            if (!parent || !parent->layout()) continue;
            auto *label = new QLabel(outputWidgetText(widget), parent);
            label->setAlignment(Qt::AlignCenter);
            label->setWordWrap(true);
            if (auto *item = parent->layout()->replaceWidget(widget,label)) delete item;
            widget->hide();
            label->show();
        }
        m_tankPanel->setStyleSheet(QStringLiteral("QWidget {background:white; color:black;}"));
    }
    fitColumnsToViewport();
}

void SchemeCalculationSheet::printTo(QPrinter *printer) const
{
    if (!printer || !m_results || m_results->rowCount() == 0) return;
    QPainter painter(printer);
    if (!painter.isActive()) return;
    const int rows = m_results->rowCount(), cols = m_results->columnCount();
    QVector<qreal> x(cols+1,0), y(rows+1,0);
    for (int c=0;c<cols;++c) x[c+1]=x[c]+m_results->columnWidth(c);
    for (int r=0;r<rows;++r) y[r+1]=y[r]+m_results->rowHeight(r);
    if (x.last() <= 0 || y.last() <= 0) return;
    const QRectF page(QPointF(0,0), printer->pageRect(QPrinter::DevicePixel).size());
    const qreal scale = std::min(page.width()/x.last(), page.height()/y.last());
    painter.translate(page.left()+(page.width()-x.last()*scale)/2, page.top());
    painter.scale(scale,scale);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = m_results->font();
    font.setPixelSize(13);
    painter.setFont(font);
    QVector<bool> covered(rows*cols,false);
    for (int r=0;r<rows;++r) for (int c=0;c<cols;++c) {
        if (covered[r*cols+c]) continue;
        const int rs=m_results->rowSpan(r,c), cs=m_results->columnSpan(r,c);
        for (int rr=r;rr<r+rs;++rr) for (int cc=c;cc<c+cs;++cc) covered[rr*cols+cc]=true;
        const QRectF rect(x[c],y[r],x[c+cs]-x[c],y[r+rs]-y[r]);
        painter.fillRect(rect,Qt::white);
        painter.setPen(QPen(Qt::black,0.6));
        painter.drawRect(rect);
        if (auto *widget=m_results->cellWidget(r,c)) {
            const QRectF contentRect = widget->property("worksheetLabelBlock").toBool()
                ? rect : rect.adjusted(2,2,-2,-2);
            widget->resize(qRound(contentRect.width()),qRound(contentRect.height()));
            widget->ensurePolished();
            if (widget->layout()) widget->layout()->activate();
            QImage image(widget->size()*3,QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(3);
            image.fill(Qt::white);
            QPainter imagePainter(&image);
            widget->render(&imagePainter);
            imagePainter.end();
            painter.drawImage(contentRect,image);
        } else if (auto *cell=m_results->item(r,c)) {
            painter.drawText(rect.adjusted(3,2,-3,-2),Qt::AlignCenter|Qt::TextWordWrap,cell->text());
        }
    }
}


bool SchemeCalculationSheet::exportExcel(const QString &path, const CalcInput &input,
        const TransformerParams &params, const CalcResult &result, QString &error) const
{
    QJsonObject cells;
    const auto address=[](int row,int col) {
        QString name;for(++col;col>0;col=(col-1)/26)name.prepend(QChar('A'+(col-1)%26));
        return name+QString::number(row+1);
    };
    for(const auto &binding:m_bindings) {
        auto *source=m_input->item(binding.row,binding.col);
        auto *widget=m_input->cellWidget(binding.row,binding.col);
        if(!widget&&(!source||!(source->flags()&Qt::ItemIsEditable)))continue;
        cells.insert(address(binding.cell->row(),binding.cell->column()),widget?outputWidgetText(widget):source->text());
    }
    cells.insert("H1",input.capacity_kVA);cells.insert("J1",input.hvRated_kV);cells.insert("L1",input.lvRated_kV);
    cells.insert("L3",input.hvTapPlusSteps);
    cells.insert("N3",input.hvTapStep_pct/100.0);
    const auto modelMatch=QRegularExpression(QStringLiteral("^(.+?)-[0-9]+(?:\\.[0-9]+)?/")).match(params.productModel);
    if(modelMatch.hasMatch())cells.insert("F1",modelMatch.captured(1).section(QLatin1Char('-'),0,0));
    cells.insert("AN3",params.nonstandardAn3_pct);
    cells.insert("AM8",input.refFluxDens_T);
    const QStringList grades={"18SQGD065","20R070","23R080","23RK085","27RK090","27ZH100","30Q120","30Q130"};
    const QStringList insulation={"ZB-0.3","ZB-0.45","QQ-2","QZB-2/130","QQL-2","ZLB-0.3","ZLB-0.45"};
    const int steel=grades.indexOf(input.steelGrade.toUpper());
    const QString insType=input.resolvedInsulationType();
    const int ins=insType==QStringLiteral("Custom")?7:(insType==QStringLiteral("RoundTable")?2:insulation.indexOf(insType));
    if(steel<0||ins<0||input.coreShape>StructureConfig::Ellipse){error=QStringLiteral("当前牌号、绝缘或心柱不在原Excel模板选项中，不能按其他选项代替导出。");return false;}
    QJsonObject controls{{"C2",input.hvCoilFormIdx},{"U10",1},{"J2",steel+1},
        {"G2",input.hvCopperWire?1:2},{"G9",input.lvCopperFoil?1:2},
        {"N2",input.yokeShape==StructureConfig::TYoke?2:1},
        {"R10",input.hvDeltaConnected?(input.lvStarConnected?3:4):(input.lvStarConnected?1:2)},
        {"U2",params.standardMode==TransformerParams::StandardMode::Custom?2:1},
        {"N9",input.coreShape==StructureConfig::Ellipse?3:(input.coreShape==StructureConfig::Circle?1:2)},
        {"X2",ins+1}};
    QJsonObject spec{{"cells",cells},{"controls",controls},{"input",SchemeStore::toJson(input)},
        {"manualCurrent",params.hasManualNoLoadCurrent()},{"currentStandard",params.noLoadCurrentStd_pct},
        {"wavePitch",input.wavePitch_mm},{"tapMinusSteps",input.hvTapMinusSteps},{"customInsulation",insType==QStringLiteral("Custom")},
        {"insulationIncrement",input.hvWireInsulAdd_mm},
        {"engine",QJsonObject{{"materialCost",result.cost.materialCost},{"noLoadLoss",result.core.noLoadLoss_W},
            {"loadLoss",result.winding.loadLoss_W},{"impedance",result.impedance.impedance_pct}}},
        {"dataNote",QStringLiteral("查表数据来自计算用数据表.xls。硅钢性能P54采用源数据1.608，原工作簿外链缓存125未沿用。Excel按原表公式重算，程序结果仅供对照。")}};
    QTemporaryDir temporary;
    if(!temporary.isValid()){error=QStringLiteral("无法建立导出临时目录");return false;}
    const auto copyResource=[&](const QString &resource,const QString &file){return QFile::copy(resource,temporary.filePath(file));};
    if(!copyResource(":/templates/linked_calculation_sheet.xlsx","template.xlsx")
        ||!copyResource(":/scripts/export_calculation_sheet.ps1","export.ps1")){error=QStringLiteral("导出模板资源不可用");return false;}
    QFile json(temporary.filePath("spec.json"));
    if(!json.open(QIODevice::WriteOnly)){error=json.errorString();return false;}
    json.write(QJsonDocument(spec).toJson());json.close();
    QProcess process;
    process.start("powershell.exe",{"-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",temporary.filePath("export.ps1"),
        "-Template",temporary.filePath("template.xlsx"),"-Spec",json.fileName(),"-Output",temporary.filePath("result.xlsx")});
    if(!process.waitForFinished(60000)){process.kill();process.waitForFinished();error=QStringLiteral("Excel导出超时");return false;}
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0){error=QString::fromLocal8Bit(process.readAllStandardError());return false;}
    // Replace the destination only after the temporary workbook is complete.
    QFile complete(temporary.filePath("result.xlsx"));
    if(!complete.open(QIODevice::ReadOnly)){error=complete.errorString();return false;}
    QSaveFile target(path);
    if(!target.open(QIODevice::WriteOnly)){error=target.errorString();return false;}
    const QByteArray bytes=complete.readAll();
    if(target.write(bytes)!=bytes.size()||!target.commit()){error=target.errorString();return false;}
    return true;
}
