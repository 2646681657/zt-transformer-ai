#include "SchemeCalculationSheet.h"
#include "ParamTableWidget.h"
#include "EmResultPanel.h"
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
#include <algorithm>

namespace {
enum Zone { Basic, Performance, Thermal, Core, High, Low, Duct, Tank, Reference, ZoneCount };
struct Field {
    QString name, value, tip;
    int row = -1, col = -1;
    bool wide = false;
};
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
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(3);
    m_resultStatus = new QLabel(this);
    m_resultStatus->setWordWrap(true);
    m_resultStatus->setStyleSheet(QStringLiteral("color:#185C37; padding:3px;"));
    outer->addWidget(m_resultStatus);
    // 唯一输入数据源保持原行列、校验、控件和联动，仅隐藏其旧布局。
    m_input = new ParamTableWidget(this);
    m_input->loadParamsForConfig(params, config, input, proMode);
    m_input->hide();
    m_results = new QTableWidget(this);
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
        m_results->setColumnWidth(col, col % 2 == 0 ? 128 : 94);
        m_results->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Interactive);
    }
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
        d->setDecimals(s->decimals()); d->setRange(s->minimum(), s->maximum()); d->setSingleStep(s->singleStep());
        d->setPrefix(s->prefix()); d->setSuffix(s->suffix()); d->setValue(s->value());
        connect(d, &QDoubleSpinBox::valueChanged, s, [s](double value) { s->setValue(value); });
        connect(s, &QDoubleSpinBox::valueChanged, d, [s,d]() { const QSignalBlocker block(d); d->setValue(s->value()); });
        copy = d;
    } else if (auto *s = qobject_cast<QSpinBox *>(source)) {
        auto *d = new QSpinBox(m_results);
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
    if (m_snapshot.valid) {
        for (const auto &group : EmResultPanel::buildGroups(m_snapshot)) {
            for (const auto &values : group.second) {
                const QString value = values.value(1) + (values.value(2).isEmpty() ? QString() : QLatin1Char(' ') + values.value(2));
                fields[resultZone(group.first, values.value(0))].append({values.value(0), value, values.value(0) + QStringLiteral("\n计算值（只读）：") + value});
            }
        }
    }
    const QStringList titles = {QStringLiteral("基本信息"), QStringLiteral("性能指标与损耗"), QStringLiteral("温升"),
        QStringLiteral("铁芯"), QStringLiteral("高压绕组"), QStringLiteral("低压绕组"),
        QStringLiteral("主空道与阻抗"), QStringLiteral("油箱、重量与成本"), QStringLiteral("工艺与参考")};
    const int scroll = m_results->verticalScrollBar()->value();
    const QSignalBlocker blocker(m_results);
    m_bindings.clear(); m_editors.clear();
    m_results->clearSpans(); m_results->setRowCount(0);
    int start = 0;
    for (int band = 0; band < 3; ++band) {
        int end = start + 1;
        m_results->setRowCount(end);
        for (int block = 0; block < 3; ++block) {
            const int zone = band * 3 + block, base = block * 4;
            auto *title = new QTableWidgetItem(titles[zone]);
            title->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            title->setBackground(QColor("#DCEEDF")); title->setForeground(QColor("#185C37"));
            QFont font = title->font(); font.setBold(true); title->setFont(font);
            m_results->setItem(start, base, title); m_results->setSpan(start, base, 1, 4);
            int row = start + 1, slot = 0;
            for (const auto &f : fields[zone]) {
                if (f.wide && slot) { ++row; slot = 0; }
                if (row >= m_results->rowCount()) m_results->setRowCount(row + 1);
                const int col = base + slot * 2;
                auto *name = new QTableWidgetItem(f.name);
                name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); name->setToolTip(f.tip);
                m_results->setItem(row, col, name);
                auto *value = new QTableWidgetItem(f.value);
                value->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); value->setToolTip(f.tip);
                m_results->setItem(row, col + 1, value);
                if (f.wide) m_results->setSpan(row, col + 1, 1, 3);
                if (f.row >= 0) {
                    m_bindings.append({f.row, f.col, value});
                    if (auto *source = m_input->cellWidget(f.row, f.col)) {
                        if (auto *editor = mirrorEditor(source)) m_results->setCellWidget(row, col + 1, editor);
                    }
                }
                if (f.wide || slot == 1) { ++row; slot = 0; } else slot = 1;
            }
            end = std::max(end, row + (slot ? 1 : 0));
        }
        start = end;
    }
    m_results->setRowCount(start);
    // 分区间的空白格也只读，不允许在空位建立无绑定的输入。
    for (int row = 0; row < start; ++row) {
        for (int col = 0; col < 12; ++col) {
            if (!m_results->item(row, col)) {
                auto *blank = new QTableWidgetItem;
                blank->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                m_results->setItem(row, col, blank);
            }
        }
    }
    syncInputs(); m_results->verticalScrollBar()->setValue(scroll);
}

void SchemeCalculationSheet::markPending()
{
    m_resultStatus->setText(QStringLiteral("浅绿：可输入　白色：只读　计算结果为上次快照，输入已修改，待重算"));
}

void SchemeCalculationSheet::loadResult(const CalcResult &result)
{
    m_snapshot = result;
    rebuildSheet();
    m_resultStatus->setText(result.valid ? QStringLiteral("浅绿：可输入　白色：只读　输入与计算值按分区就近排列（当前方案）")
        : QStringLiteral("计算结果不可用，请计算当前方案；浅绿单元格可输入"));
}
