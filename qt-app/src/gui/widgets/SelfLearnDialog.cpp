#include "SelfLearnDialog.h"
#include "TestReportDialog.h"
#include "SchemePickDialog.h"
#include "ElectromagneticEngine.h"
#include "CalcResult.h"
#include <functional>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>

// 对比指标定义：名称 / 设计值( CalcResult 字段访问器 ) / 实测值 / 单位 / 容差
namespace {

struct MetricDef {
    QString name;
    std::function<double(const CalcResult &)> design;
    std::function<double(const SchemeStore::TestReport &)> measured;
    QString unit;
    double tolerance;      // 允许偏差 ±%，超出高亮
};

// 六项核心指标（损耗/阻抗/温升——试验报告标准项目）
QVector<MetricDef> metricDefs()
{
    return {
        { QStringLiteral("空载损耗"),
          [](const CalcResult &r) { return r.core.noLoadLoss_W; },
          [](const SchemeStore::TestReport &t) { return t.noLoadLoss_W; },
          QStringLiteral("W"), 10.0 },
        { QStringLiteral("负载损耗"),
          [](const CalcResult &r) { return r.winding.loadLoss_W; },
          [](const SchemeStore::TestReport &t) { return t.loadLoss_W; },
          QStringLiteral("W"), 10.0 },
        { QStringLiteral("阻抗电压"),
          [](const CalcResult &r) { return r.impedance.impedance_pct; },
          [](const SchemeStore::TestReport &t) { return t.impedance_pct; },
          QStringLiteral("%"), 10.0 },
        { QStringLiteral("油顶层温升"),
          [](const CalcResult &r) { return r.thermal.oilTopRise_K; },
          [](const SchemeStore::TestReport &t) { return t.oilTopRise_K; },
          QStringLiteral("K"), 10.0 },
        { QStringLiteral("高压绕组温升"),
          [](const CalcResult &r) { return r.thermal.hvWindingRise_K; },
          [](const SchemeStore::TestReport &t) { return t.hvWindingRise_K; },
          QStringLiteral("K"), 10.0 },
        { QStringLiteral("低压绕组温升"),
          [](const CalcResult &r) { return r.thermal.lvWindingRise_K; },
          [](const SchemeStore::TestReport &t) { return t.lvWindingRise_K; },
          QStringLiteral("K"), 10.0 },
    };
}

} // namespace

SelfLearnDialog::SelfLearnDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("AI 自学习 - 设计值与实测值对比"));
    setModal(true);
    resize(560, 420);
    setupUi();
    refreshCompare();   // 初始未选方案时显示提示状态
}

void SelfLearnDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // 顶部操作行：选方案 / 录实测 / 重新对比
    auto *btnRow = new QHBoxLayout;
    m_pickBtn = new QPushButton(QStringLiteral("1. 选择方案"), this);
    m_editBtn = new QPushButton(QStringLiteral("2. 录入实测数据"), this);
    m_calcBtn = new QPushButton(QStringLiteral("3. 重新对比"), this);
    m_pickBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_calcBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pickBtn, &QPushButton::clicked, this, &SelfLearnDialog::onPickScheme);
    connect(m_editBtn, &QPushButton::clicked, this, &SelfLearnDialog::onEditReport);
    connect(m_calcBtn, &QPushButton::clicked, this, &SelfLearnDialog::refreshCompare);
    btnRow->addWidget(m_pickBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_calcBtn);
    btnRow->addStretch(1);
    layout->addLayout(btnRow);

    // 方案信息行
    m_infoLabel = new QLabel(QStringLiteral("未选择方案"), this);
    m_infoLabel->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    layout->addWidget(m_infoLabel);

    const QVector<MetricDef> defs = metricDefs();
    m_table = new QTableWidget(defs.size(), 5, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("指标"), QStringLiteral("设计值"),
        QStringLiteral("实测值"), QStringLiteral("偏差"),
        QStringLiteral("结论")});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    for (int i = 0; i < defs.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(defs[i].name));
    }
    layout->addWidget(m_table, 1);

    auto *hint = new QLabel(
        QStringLiteral("偏差 = (实测-设计)/设计×100%；|偏差| > 10% 标红（损耗/阻抗/温升通用工程容差）。"
                       "修正系数自动调整需积累多组对比数据后开放"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    layout->addWidget(hint);
}

void SelfLearnDialog::onPickScheme()
{
    // 方案库 + 记忆库合并列表
    QVector<SchemeStore::SchemeEntry> entries =
        SchemeStore::loadEntries(SchemeStore::mySchemesPath());
    entries += SchemeStore::loadEntries(SchemeStore::memorySchemesPath());

    if (entries.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("无可用方案"),
            QStringLiteral("方案库与记忆库均为空，请先在参数设置页保存方案或进入计算"));
        return;
    }

    SchemePickDialog dlg(QStringLiteral("选择对比方案"), entries, QString(), this);
    if (dlg.exec() == QDialog::Accepted && dlg.hasSelection()) {
        m_entry = dlg.selectedEntry();
        m_hasScheme = true;
        refreshCompare();
    }
}

void SelfLearnDialog::onEditReport()
{
    if (!m_hasScheme) {
        QMessageBox::information(this, QStringLiteral("未选择方案"),
            QStringLiteral("请先选择方案，再录入该方案的试验实测数据"));
        return;
    }
    TestReportDialog dlg(m_entry.test, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_entry.test = dlg.report();
        // 实测数据写回方案库（同名同变量的条目更新，下次打开仍带数据）
        const auto updateStore = [](const QString &path,
                                    const SchemeStore::SchemeEntry &updated) {
            QVector<SchemeStore::SchemeEntry> list = SchemeStore::loadEntries(path);
            for (auto &e : list) {
                if (e.name == updated.name
                    && SchemeStore::sameInput(e.input, updated.input)) {
                    e.test = updated.test;
                    break;
                }
            }
            SchemeStore::saveEntries(path, list);
        };
        updateStore(SchemeStore::mySchemesPath(), m_entry);
        updateStore(SchemeStore::memorySchemesPath(), m_entry);
        refreshCompare();
    }
}

void SelfLearnDialog::refreshCompare()
{
    if (!m_hasScheme) {
        m_infoLabel->setText(QStringLiteral("未选择方案——点击上方「选择方案」开始"));
        for (int i = 0; i < m_table->rowCount(); ++i) {
            for (int c = 1; c < 5; ++c) {
                m_table->setItem(i, c, new QTableWidgetItem(QStringLiteral("--")));
            }
        }
        return;
    }

    // 引擎重算设计值（确定性：同输入同输出）
    ElectromagneticEngine engine;
    CalcResult result;
    if (!engine.calcElectromagnetic(m_entry.input, result)) {
        QMessageBox::warning(this, QStringLiteral("计算失败"),
            QStringLiteral("该方案设计变量无法完成电磁计算，请检查方案数据"));
        return;
    }
    fillCompareTable(result);

    const QString testInfo = m_entry.test.hasData
        ? QStringLiteral("已录入（%1）").arg(m_entry.test.testedAt.date().toString(
              QStringLiteral("yyyy-MM-dd")))
        : QStringLiteral("未录入——点击「录入实测数据」");
    m_infoLabel->setText(QStringLiteral("方案「%1」 | 实测数据：%2")
                             .arg(m_entry.name, testInfo));
}

void SelfLearnDialog::fillCompareTable(const CalcResult &result)
{
    const QVector<MetricDef> defs = metricDefs();
    for (int i = 0; i < defs.size() && i < m_table->rowCount(); ++i) {
        const MetricDef &m = defs[i];
        const double design = m.design(result);
        const double measured = m_entry.test.hasData ? m.measured(m_entry.test) : 0.0;

        m_table->setItem(i, 1, new QTableWidgetItem(
            QString::number(design, 'f', 1) + QStringLiteral(" ") + m.unit));

        if (!m_entry.test.hasData) {
            m_table->setItem(i, 2, new QTableWidgetItem(QStringLiteral("待录入")));
            m_table->setItem(i, 3, new QTableWidgetItem(QStringLiteral("--")));
            m_table->setItem(i, 4, new QTableWidgetItem(QStringLiteral("--")));
            continue;
        }

        m_table->setItem(i, 2, new QTableWidgetItem(
            QString::number(measured, 'f', 1) + QStringLiteral(" ") + m.unit));

        const double dev = design > 0 ? (measured - design) / design * 100.0 : 0.0;
        auto *devItem = new QTableWidgetItem(
            QStringLiteral("%1%").arg(QString::number(dev, 'f', 1)));
        auto *verdict = new QTableWidgetItem(
            qAbs(dev) > m.tolerance ? QStringLiteral("超差") : QStringLiteral("合格"));

        if (qAbs(dev) > m.tolerance) {
            // 超差高亮（红底白字）
            devItem->setBackground(QColor(183, 28, 28));
            devItem->setForeground(Qt::white);
            verdict->setBackground(QColor(183, 28, 28));
            verdict->setForeground(Qt::white);
        }
        m_table->setItem(i, 3, devItem);
        m_table->setItem(i, 4, verdict);
    }
}
