#include "DataQueryPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QHeaderView>

#include "DesignDatabase.h"

namespace {

// 数据类别（与左侧分类树一一对应）
enum DataCategory {
    CatSteelCurve = 0,  // 硅钢性能曲线（子级为具体牌号）
    CatPerfStd,         // 性能标准值
    CatCoreLam,         // 铁芯叠积对照表
    CatWireSpec,        // 线规表
    CatCorrCoef         // 波纹油箱系数
};

// 树节点自定义数据角色
const int RoleCategory = Qt::UserRole + 1;   // DataCategory
const int RoleGradeIdx = Qt::UserRole + 2;   // 硅钢牌号序号

// 数字转文本：保留最多 4 位小数并去掉尾零（0 显示为 "0"）
QString num(double v)
{
    QString s = QString::number(v, 'f', 4);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s;
}

} // namespace

DataQueryPage::DataQueryPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    loadTree();
}

void DataQueryPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 工具栏：页面名 + 搜索过滤 + 计数
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet("background: #2a2f38; border-bottom: 1px solid #3a4050;");
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(8, 4, 8, 4);
    toolLayout->setSpacing(8);

    auto *pageLabel = new QLabel(QStringLiteral("数据查询"), toolbar);
    pageLabel->setStyleSheet("color: #4dd0e1; font-size: 12px; font-weight: bold;");
    toolLayout->addWidget(pageLabel);

    toolLayout->addStretch();

    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText(QStringLiteral("输入关键字过滤当前表格…"));
    m_searchEdit->setFixedWidth(220);
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &DataQueryPage::onSearchChanged);
    toolLayout->addWidget(m_searchEdit);

    m_countLabel = new QLabel(toolbar);
    m_countLabel->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    toolLayout->addWidget(m_countLabel);

    mainLayout->addWidget(toolbar);

    // 主区域：左侧分类树 + 右侧数据表格
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(splitter);
    m_tree->setHeaderHidden(true);
    m_tree->setFixedWidth(200);
    m_tree->setStyleSheet(
        "QTreeWidget { background: #22262e; color: #e0e6ed; border: 1px solid #3a4050;"
        "font-size: 12px; }"
        "QTreeWidget::item { height: 26px; border: none; }"
        "QTreeWidget::item:hover { background: rgba(0,188,212,0.1); }"
        "QTreeWidget::item:selected { background: rgba(0,188,212,0.25); color: #ffffff; }");
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, &DataQueryPage::onTreeCurrentChanged);
    splitter->addWidget(m_tree);

    m_table = new QTableWidget(splitter);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);   // 只读
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(true);
    splitter->addWidget(m_table);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter, 1);
}

void DataQueryPage::loadTree()
{
    DesignDatabase &db = DesignDatabase::instance();
    if (!db.load()) {
        m_countLabel->setText(QStringLiteral("数据加载失败"));
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, db.lastError());
        return;
    }

    // 硅钢性能曲线：父节点 + 各牌号子节点
    auto *steelRoot = new QTreeWidgetItem(m_tree);
    steelRoot->setText(0, QStringLiteral("硅钢性能曲线"));
    const auto &curves = db.steelCurves();
    for (int i = 0; i < curves.size(); ++i) {
        auto *child = new QTreeWidgetItem(steelRoot);
        child->setText(0, curves[i].grade);
        child->setData(0, RoleCategory, CatSteelCurve);
        child->setData(0, RoleGradeIdx, i);
    }
    steelRoot->setExpanded(true);

    auto addLeaf = [this](const QString &text, int category) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, text);
        item->setData(0, RoleCategory, category);
        item->setData(0, RoleGradeIdx, -1);
    };
    addLeaf(QStringLiteral("性能标准值"), CatPerfStd);
    addLeaf(QStringLiteral("铁芯叠积对照表"), CatCoreLam);
    addLeaf(QStringLiteral("线规表"), CatWireSpec);
    addLeaf(QStringLiteral("波纹油箱系数"), CatCorrCoef);

    // 默认选中第一个牌号
    if (steelRoot->childCount() > 0)
        m_tree->setCurrentItem(steelRoot->child(0));
}

void DataQueryPage::onTreeCurrentChanged(QTreeWidgetItem *current)
{
    if (!current)
        return;
    // 父节点（无类别数据）：选中其第一个子节点
    if (!current->data(0, RoleCategory).isValid()) {
        if (current->childCount() > 0)
            m_tree->setCurrentItem(current->child(0));
        return;
    }
    const int category = current->data(0, RoleCategory).toInt();
    const int gradeIdx = current->data(0, RoleGradeIdx).toInt();
    showTable(category, gradeIdx);
}

void DataQueryPage::showTable(int category, int gradeIdx)
{
    DesignDatabase &db = DesignDatabase::instance();
    // clearContents 只清单元格内容不清行数，必须显式清零，
    // 否则旧空壳行残留，新数据被 appendRow 追加到可视区之外
    m_table->clearContents();
    m_table->setRowCount(0);
    m_searchEdit->clear();

    auto setColumns = [this](const QStringList &headers) {
        m_table->setColumnCount(headers.size());
        m_table->setHorizontalHeaderLabels(headers);
    };
    auto appendRow = [this](const QStringList &cells) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        for (int col = 0; col < cells.size(); ++col)
            m_table->setItem(row, col, new QTableWidgetItem(cells[col]));
    };

    switch (category) {
    case CatSteelCurve: {
        const auto &curves = db.steelCurves();
        if (gradeIdx < 0 || gradeIdx >= curves.size())
            break;
        setColumns({QStringLiteral("序号"),
                    QStringLiteral("磁密 B (T)"),
                    QStringLiteral("单位铁损 (W/kg)"),
                    QStringLiteral("磁化容量 (VA/kg)")});
        for (const auto &pt : curves[gradeIdx].points) {
            appendRow({num(pt.index), num(pt.b), num(pt.wPerKg), num(pt.vaPerKg)});
        }
        break;
    }
    case CatPerfStd: {
        setColumns({QStringLiteral("容量 (kVA)"),
                    QStringLiteral("空载电流标准 (%)")});
        for (const auto &row : db.perfStandards()) {
            appendRow({num(row.first), num(row.second)});
        }
        break;
    }
    case CatCoreLam: {
        const auto &rows = db.coreRows();
        // 列数取各行的最大值（片宽 11 级 + 轭片基数 4 列）
        int maxW = 0, maxY = 0;
        for (const auto &r : rows) {
            maxW = qMax(maxW, r.widths.size());
            maxY = qMax(maxY, r.yokeBase.size());
        }
        QStringList headers{QStringLiteral("参考直径 (mm)")};
        for (int i = 0; i < maxW; ++i)
            headers << QStringLiteral("片宽%1").arg(i + 1);
        for (int i = 0; i < maxY; ++i)
            headers << QStringLiteral("轭基%1").arg(i + 1);
        setColumns(headers);
        for (const auto &r : rows) {
            QStringList cells{num(r.ref)};
            for (double w : r.widths)
                cells << (w > 0.0 ? num(w) : QStringLiteral("-"));
            for (double y : r.yokeBase)
                cells << (y > 0.0 ? num(y) : QStringLiteral("-"));
            appendRow(cells);
        }
        break;
    }
    case CatWireSpec: {
        setColumns({QStringLiteral("裸线宽 (mm)"),
                    QStringLiteral("绝缘后宽 (mm)"),
                    QStringLiteral("导线加重量 (%)")});
        for (const auto &spec : db.wireSpecs()) {
            appendRow({num(spec.bareWidthMm), num(spec.insulatedWidthMm),
                       num(spec.weightAddPct)});
        }
        break;
    }
    case CatCorrCoef: {
        setColumns({QStringLiteral("波纹深 (mm)"),
                    QStringLiteral("Ks"),
                    QStringLiteral("Kp")});
        for (const auto &row : db.corrCoefs()) {
            appendRow({num(row.value(0)), num(row.value(1)), num(row.value(2))});
        }
        break;
    }
    default:
        break;
    }

    m_table->scrollToTop();
    applyFilter();
}

void DataQueryPage::onSearchChanged(const QString &text)
{
    Q_UNUSED(text);
    applyFilter();
}

// 按关键字过滤行（整行任意列包含关键字即保留，不区分大小写）
void DataQueryPage::applyFilter()
{
    const QString key = m_searchEdit->text().trimmed();
    int visible = 0;
    const int rows = m_table->rowCount();
    for (int r = 0; r < rows; ++r) {
        bool match = key.isEmpty();
        if (!match) {
            const int cols = m_table->columnCount();
            for (int c = 0; c < cols; ++c) {
                const QTableWidgetItem *item = m_table->item(r, c);
                if (item && item->text().contains(key, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
        }
        m_table->setRowHidden(r, !match);
        if (match)
            ++visible;
    }
    if (key.isEmpty())
        m_countLabel->setText(QStringLiteral("共 %1 项").arg(rows));
    else
        m_countLabel->setText(QStringLiteral("匹配 %1 / %2 项").arg(visible).arg(rows));
}
