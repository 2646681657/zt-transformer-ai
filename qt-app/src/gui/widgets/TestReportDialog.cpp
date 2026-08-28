#include "TestReportDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QLabel>
#include <QPushButton>

TestReportDialog::TestReportDialog(const SchemeStore::TestReport &report,
                                   QWidget *parent)
    : QDialog(parent), m_report(report)
{
    setWindowTitle(QStringLiteral("录入试验实测数据"));
    setModal(true);
    setupUi();
}

void TestReportDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(6);

    const auto makeSpin = [this](double min, double max, double step,
                                 double value, const QString &suffix) {
        auto *spin = new QDoubleSpinBox(this);
        spin->setRange(min, max);
        spin->setDecimals(1);
        spin->setSingleStep(step);
        spin->setValue(value);
        if (!suffix.isEmpty()) {
            spin->setSuffix(suffix);
        }
        return spin;
    };

    m_noLoadLoss = makeSpin(0, 100000, 10, m_report.noLoadLoss_W, QStringLiteral(" W"));
    m_loadLoss = makeSpin(0, 500000, 50, m_report.loadLoss_W, QStringLiteral(" W"));
    m_impedance = makeSpin(0, 30, 0.1, m_report.impedance_pct, QStringLiteral(" %"));
    m_oilTopRise = makeSpin(0, 120, 0.5, m_report.oilTopRise_K, QStringLiteral(" K"));
    m_hvRise = makeSpin(0, 120, 0.5, m_report.hvWindingRise_K, QStringLiteral(" K"));
    m_lvRise = makeSpin(0, 120, 0.5, m_report.lvWindingRise_K, QStringLiteral(" K"));

    m_testDate = new QDateEdit(this);
    m_testDate->setCalendarPopup(true);
    m_testDate->setDate(m_report.testedAt.isValid()
                            ? m_report.testedAt.date()
                            : QDate::currentDate());

    form->addRow(QStringLiteral("实测空载损耗:"), m_noLoadLoss);
    form->addRow(QStringLiteral("实测负载损耗:"), m_loadLoss);
    form->addRow(QStringLiteral("实测阻抗电压:"), m_impedance);
    form->addRow(QStringLiteral("实测油顶层温升:"), m_oilTopRise);
    form->addRow(QStringLiteral("实测高压绕组温升:"), m_hvRise);
    form->addRow(QStringLiteral("实测低压绕组温升:"), m_lvRise);
    form->addRow(QStringLiteral("试验日期:"), m_testDate);
    layout->addLayout(form);

    auto *hint = new QLabel(
        QStringLiteral("提示：对照设计值可验证计算准确性，超差项将在自学习对比中高亮"), this);
    hint->setStyleSheet("color: #8a9bb0; font-size: 11px;");
    layout->addWidget(hint);

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(btnBox, &QDialogButtonBox::accepted, this, &TestReportDialog::onSave);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);
}

void TestReportDialog::onSave()
{
    m_report.noLoadLoss_W = m_noLoadLoss->value();
    m_report.loadLoss_W = m_loadLoss->value();
    m_report.impedance_pct = m_impedance->value();
    m_report.oilTopRise_K = m_oilTopRise->value();
    m_report.hvWindingRise_K = m_hvRise->value();
    m_report.lvWindingRise_K = m_lvRise->value();
    m_report.testedAt = QDateTime(m_testDate->date(), QTime(0, 0));
    m_report.hasData = true;   // 保存即视为已录入
    accept();
}
