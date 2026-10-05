#include "SchemeCalculationSheet.h"
#include "ParamTableWidget.h"
#include "EmResultPanel.h"
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QScrollBar>
#include <QSignalBlocker>

namespace {
// 根据现有可编辑标志绘制，不改行列、单元格值或参数键绑定。
class SheetInputDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        const auto heading = index.siblingAtColumn(1);
        if (heading.data(Qt::FontRole).value<QFont>().bold()) {
            option->backgroundBrush = QColor(QStringLiteral("#DCEEDF"));
            option->palette.setColor(QPalette::Text, QColor(QStringLiteral("#185C37")));
        } else {
            const bool editable = index.flags().testFlag(Qt::ItemIsEditable);
            option->backgroundBrush = QColor(editable ? QStringLiteral("#F0FAF2") : QStringLiteral("#FFFFFF"));
            option->palette.setColor(QPalette::Text, QColor(QStringLiteral("#24362B")));
        }
    }
};

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("background:#217346; color:white; font-weight:600; padding:7px 10px;"));
    label->setWordWrap(true);
    return label;
}
}

SchemeCalculationSheet::SchemeCalculationSheet(const TransformerParams &params,
        const StructureConfig &config, const CalcInput &input, const CalcResult &result,
        bool proMode, QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(6);
    auto *legend = new QLabel(QStringLiteral("浅绿单元格 / 下拉框：可输入　白色单元格：名称或只读值　右侧结果：点击“计算”后刷新"), this);
    legend->setWordWrap(true);
    legend->setStyleSheet(QStringLiteral("color:#486152; padding:3px 0;"));
    outer->addWidget(legend);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    outer->addWidget(splitter, 1);

    auto *inputPane = new QWidget(splitter);
    auto *inputLayout = new QVBoxLayout(inputPane);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(0);
    inputLayout->addWidget(sectionTitle(QStringLiteral("设计参数 · 仅修改当前方案"), inputPane));
    m_input = new ParamTableWidget(inputPane);
    m_input->loadParamsForConfig(params, config, input, proMode);
    m_input->setItemDelegate(new SheetInputDelegate(m_input));
    m_input->setColumnHidden(0, true);
    m_input->verticalHeader()->setVisible(true);
    m_input->verticalHeader()->setDefaultSectionSize(32);
    m_input->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("参数名称"), QStringLiteral("输入 / 联动值"),
        QStringLiteral("参数 / 选项"), QStringLiteral("输入 / 联动值"), QStringLiteral("备注 / 计算依据")});
    m_input->setColumnWidth(1, 166);
    m_input->setColumnWidth(2, 120);
    m_input->setColumnWidth(3, 174);
    m_input->setColumnWidth(4, 120);
    m_input->horizontalHeader()->setMinimumSectionSize(60);
    m_input->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_input->setWordWrap(false);
    m_input->setAlternatingRowColors(false);
    m_input->setShowGrid(true);
    // 样式只作用于这个实例及其编辑器，不修改公共组件或应用主题。
    const QString gridStyle = QStringLiteral(
        "QTableWidget { background:white; color:#24362B; gridline-color:#D3DDD6; border:1px solid #C4D7C9;"
        " selection-background-color:#CDE8D5; selection-color:#185C37; }"
        "QHeaderView::section { background:#E7F2EA; color:#185C37; border:1px solid #D3DDD6; padding:5px; }"
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background:#F0FAF2; color:#24362B;"
        " border:1px solid #A8CDB3; padding:2px; min-height:22px; }"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border:1px solid #217346; }"
        "QComboBox QAbstractItemView { background:white; color:#24362B; selection-background-color:#CDE8D5; }"
        "QTableCornerButton::section { background:#E7F2EA; border:1px solid #D3DDD6; }");
    m_input->setStyleSheet(gridStyle);
    for (int row = 0; row < m_input->rowCount(); ++row) {
        for (int col : {1, 3, 5}) {
            auto *item = m_input->item(row, col);
            if (item && item->toolTip().isEmpty()) item->setToolTip(item->text());
        }
    }
    inputLayout->addWidget(m_input, 1);

    auto *resultPane = new QWidget(splitter);
    auto *resultLayout = new QVBoxLayout(resultPane);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultLayout->setSpacing(0);
    m_resultStatus = sectionTitle(QStringLiteral("计算结果 · 当前方案快照（只读）"), resultPane);
    resultLayout->addWidget(m_resultStatus);
    m_results = new QTableWidget(resultPane);
    m_results->setColumnCount(3);
    m_results->setHorizontalHeaderLabels({QStringLiteral("计算项目"), QStringLiteral("结果 / 说明"), QStringLiteral("单位")});
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setWordWrap(true);
    m_results->setStyleSheet(gridStyle);
    m_results->setColumnWidth(0, 238);
    m_results->setColumnWidth(2, 48);
    m_results->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_results->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_results->verticalHeader()->setDefaultSectionSize(30);
    resultLayout->addWidget(m_results, 1);
    splitter->setSizes({760, 590});
    splitter->setStretchFactor(0, 6);
    splitter->setStretchFactor(1, 5);
    loadResult(result);

    // 仅显示结果是否待重算；联动信号仍由原参数组件处理。
    connect(m_input, &QTableWidget::itemChanged, this, [this]() { markPending(); });
    for (auto *edit : m_input->findChildren<QLineEdit *>())
        connect(edit, &QLineEdit::textEdited, this, [this]() { markPending(); });
    for (auto *combo : m_input->findChildren<QComboBox *>())
        connect(combo, &QComboBox::currentIndexChanged, this, [this]() { markPending(); });
    for (auto *spin : m_input->findChildren<QSpinBox *>())
        connect(spin, &QSpinBox::valueChanged, this, [this]() { markPending(); });
    for (auto *spin : m_input->findChildren<QDoubleSpinBox *>())
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() { markPending(); });
}

void SchemeCalculationSheet::markPending()
{
    m_resultStatus->setText(QStringLiteral("上次计算结果 · 输入已修改 / 待重算（只读）"));
}

void SchemeCalculationSheet::loadResult(const CalcResult &result)
{
    const int scroll = m_results->verticalScrollBar()->value();
    const QSignalBlocker blocker(m_results);
    m_results->clearSpans();
    m_results->setRowCount(0);
    if (!result.valid) {
        m_resultStatus->setText(QStringLiteral("计算结果 · 不可用，请计算当前方案"));
        return;
    }
    const auto groups = EmResultPanel::buildGroups(result);
    for (const auto &group : groups) {
        int row = m_results->rowCount();
        m_results->insertRow(row);
        auto *title = new QTableWidgetItem(group.first);
        title->setFlags(title->flags() & ~Qt::ItemIsEditable);
        title->setBackground(QColor(QStringLiteral("#DCEEDF")));
        title->setForeground(QColor(QStringLiteral("#185C37")));
        QFont font = title->font();
        font.setBold(true);
        title->setFont(font);
        m_results->setItem(row, 0, title);
        m_results->setSpan(row, 0, 1, 3);
        for (const auto &values : group.second) {
            row = m_results->rowCount();
            m_results->insertRow(row);
            for (int col = 0; col < 3; ++col) {
                auto *cell = new QTableWidgetItem(values.value(col));
                cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
                cell->setToolTip(values.value(col));
                if (col == 1 && !values.value(2).isEmpty())
                    cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_results->setItem(row, col, cell);
            }
        }
    }
    m_results->resizeRowsToContents();
    m_results->verticalScrollBar()->setValue(scroll);
    m_resultStatus->setText(QStringLiteral("计算结果 · 当前方案快照（只读）"));
}
