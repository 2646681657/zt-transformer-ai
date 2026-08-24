#include "LossCalcPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QPalette>
#include <QFont>
#include <QLineEdit>
#include <cmath>

namespace {
double excelRound(double v, int digits)
{
    const double p = std::pow(10.0, digits);
    return std::floor(v * p + 0.5) / p;
}
} // namespace

LossCalcPage::LossCalcPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void LossCalcPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 工具栏
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet("background: #2a2f38; border-bottom: 1px solid #3a4050;");
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(8, 4, 8, 4);
    auto *pageLabel = new QLabel(QStringLiteral("损耗计算器"), toolbar);
    pageLabel->setStyleSheet("color: #4dd0e1; font-size: 12px; font-weight: bold;");
    toolLayout->addWidget(pageLabel);
    toolLayout->addStretch();
    mainLayout->addWidget(toolbar);

    // Tab 页：空载损耗 / 负载损耗
    auto *content = new QWidget(this);
    content->setStyleSheet("background: #1a1d23;");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 16, 16, 16);

    auto *tabs = new QTabWidget(content);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #3a4050; background: #1a1d23; }"
        "QTabBar::tab { background: #22262e; color: #8a9bb0; padding: 6px 16px;"
        " border: 1px solid #3a4050; border-bottom: none; border-top-left-radius: 4px;"
        " border-top-right-radius: 4px; font-size: 12px; }"
        "QTabBar::tab:selected { background: #2a2f38; color: #4dd0e1; }");

    tabs->addTab(createNoLoadTab(), QStringLiteral("空载损耗"));
    tabs->addTab(createLoadLossTab(), QStringLiteral("负载损耗"));
    contentLayout->addWidget(tabs);

    mainLayout->addWidget(content, 1);
}

QWidget *LossCalcPage::createNoLoadTab()
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    // 输入
    auto *paramGroup = new QGroupBox(QStringLiteral("输入参数"), page);
    paramGroup->setStyleSheet(
        "QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
        " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    auto *form = new QFormLayout(paramGroup);
    form->setContentsMargins(12, 8, 12, 12);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto makeSpin = [this](double min, double max, double val, double step, const QString &suffix) {
        auto *spin = new QDoubleSpinBox(this);
        spin->setRange(min, max);
        spin->setDecimals(2);
        spin->setSingleStep(step);
        spin->setValue(val);
        spin->setSuffix(suffix);
        spin->setFixedWidth(150);
        QPalette pal = spin->palette();
        pal.setColor(QPalette::Base, QColor(0x22, 0x26, 0x2e));
        pal.setColor(QPalette::Text, QColor(0xe0, 0xe6, 0xed));
        pal.setColor(QPalette::Button, QColor(0x2a, 0x2f, 0x38));
        pal.setColor(QPalette::ButtonText, QColor(0xc0, 0xc8, 0xd0));
        spin->setPalette(pal);
        QFont f = spin->font();
        f.setPointSize(8);
        spin->setFont(f);
        if (auto *le = spin->findChild<QLineEdit*>()) {
            le->setStyleSheet("QLineEdit { background: #22262e; color: #e0e6ed;"
                              " border: 1px solid #3a4050; border-radius: 3px; padding: 1px 4px; }");
        }
        return spin;
    };

    m_coreWeight = makeSpin(0.0, 100000.0, 395.0,  1.0, QStringLiteral(" kg"));
    m_lossPerKg  = makeSpin(0.0, 10.0,      1.35,  0.01, QStringLiteral(" W/kg"));
    m_craftCoef  = makeSpin(0.5, 3.0,       1.23,  0.01, QStringLiteral(""));

    form->addRow(QStringLiteral("硅钢片总重 (kg)"), m_coreWeight);
    form->addRow(QStringLiteral("单位铁损 (W/kg)"), m_lossPerKg);
    form->addRow(QStringLiteral("工艺系数"), m_craftCoef);

    auto *calcBtn = new QPushButton(QStringLiteral("计算"), paramGroup);
    calcBtn->setCursor(Qt::PointingHandCursor);
    calcBtn->setStyleSheet(
        "QPushButton { background: #00bcd4; color: #1a1d23; font-size: 12px;"
        " padding: 6px 24px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #4dd0e1; }");
    form->addRow(QString(), calcBtn);
    connect(calcBtn, &QPushButton::clicked, this, &LossCalcPage::onCalcNoLoad);

    layout->addWidget(paramGroup, 0, Qt::AlignTop);
    layout->addStretch();

    // 结果
    auto *resultGroup = new QGroupBox(QStringLiteral("计算结果"), page);
    resultGroup->setStyleSheet(paramGroup->styleSheet());
    auto *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setContentsMargins(12, 8, 12, 12);
    m_noLoadResult = new QLabel(QStringLiteral("空载损耗：—"), resultGroup);
    m_noLoadResult->setStyleSheet("color: #4dd0e1; font-size: 20px; font-weight: bold;");
    resultLayout->addWidget(m_noLoadResult);
    resultLayout->addStretch();
    layout->addWidget(resultGroup, 1);

    return page;
}

QWidget *LossCalcPage::createLoadLossTab()
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto *paramGroup = new QGroupBox(QStringLiteral("输入参数"), page);
    paramGroup->setStyleSheet(
        "QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
        " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    auto *form = new QFormLayout(paramGroup);
    form->setContentsMargins(12, 8, 12, 12);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto makeSpin = [this](double min, double max, double val, double step, const QString &suffix) {
        auto *spin = new QDoubleSpinBox(this);
        spin->setRange(min, max);
        spin->setDecimals(3);
        spin->setSingleStep(step);
        spin->setValue(val);
        spin->setSuffix(suffix);
        spin->setFixedWidth(150);
        QPalette pal = spin->palette();
        pal.setColor(QPalette::Base, QColor(0x22, 0x26, 0x2e));
        pal.setColor(QPalette::Text, QColor(0xe0, 0xe6, 0xed));
        pal.setColor(QPalette::Button, QColor(0x2a, 0x2f, 0x38));
        pal.setColor(QPalette::ButtonText, QColor(0xc0, 0xc8, 0xd0));
        spin->setPalette(pal);
        QFont f = spin->font();
        f.setPointSize(8);
        spin->setFont(f);
        if (auto *le = spin->findChild<QLineEdit*>()) {
            le->setStyleSheet("QLineEdit { background: #22262e; color: #e0e6ed;"
                              " border: 1px solid #3a4050; border-radius: 3px; padding: 1px 4px; }");
        }
        return spin;
    };

    m_hvResistance = makeSpin(0.0, 100.0,    0.088,  0.001, QStringLiteral(" Ω"));
    m_lvResistance = makeSpin(0.0, 100.0,    0.0007, 0.0001, QStringLiteral(" Ω"));
    m_hvCurrent    = makeSpin(0.0, 100000.0, 12.1,   0.1,   QStringLiteral(" A"));
    m_lvCurrent    = makeSpin(0.0, 100000.0, 909.3,  1.0,   QStringLiteral(" A"));
    m_hvExtraLoss  = makeSpin(0.0, 10000.0,  15.0,   1.0,   QStringLiteral(" W"));
    m_lvExtraLoss  = makeSpin(0.0, 10000.0,  0.0,    1.0,   QStringLiteral(" W"));
    m_strayFactor  = makeSpin(0.0, 50.0,     11.0,   0.5,   QStringLiteral(" %"));

    form->addRow(QStringLiteral("高压电阻 (Ω)"), m_hvResistance);
    form->addRow(QStringLiteral("低压电阻 (Ω)"), m_lvResistance);
    form->addRow(QStringLiteral("高压相电流 (A)"), m_hvCurrent);
    form->addRow(QStringLiteral("低压相电流 (A)"), m_lvCurrent);
    form->addRow(QStringLiteral("高压附加损耗 (W)"), m_hvExtraLoss);
    form->addRow(QStringLiteral("低压附加损耗 (W)"), m_lvExtraLoss);
    form->addRow(QStringLiteral("杂散损耗系数 (%)"), m_strayFactor);

    auto *calcBtn = new QPushButton(QStringLiteral("计算"), paramGroup);
    calcBtn->setCursor(Qt::PointingHandCursor);
    calcBtn->setStyleSheet(
        "QPushButton { background: #00bcd4; color: #1a1d23; font-size: 12px;"
        " padding: 6px 24px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #4dd0e1; }");
    form->addRow(QString(), calcBtn);
    connect(calcBtn, &QPushButton::clicked, this, &LossCalcPage::onCalcLoadLoss);

    layout->addWidget(paramGroup, 0, Qt::AlignTop);
    layout->addStretch();

    auto *resultGroup = new QGroupBox(QStringLiteral("计算结果"), page);
    resultGroup->setStyleSheet(paramGroup->styleSheet());
    auto *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setContentsMargins(12, 8, 12, 12);
    m_loadLossResult = new QLabel(QStringLiteral("负载损耗：—"), resultGroup);
    m_loadLossResult->setStyleSheet("color: #4dd0e1; font-size: 20px; font-weight: bold;");
    m_loadLossResult->setTextFormat(Qt::RichText);
    m_loadLossResult->setWordWrap(true);
    resultLayout->addWidget(m_loadLossResult);
    resultLayout->addStretch();
    layout->addWidget(resultGroup, 1);

    return page;
}

void LossCalcPage::onCalcNoLoad()
{
    const double weight = m_coreWeight->value();
    const double lossPerKg = m_lossPerKg->value();
    const double craftCoef = m_craftCoef->value();

    // 空载损耗 = 工艺系数 × 单位铁损 × 硅钢片总重
    const double noLoadLoss = excelRound(craftCoef * lossPerKg * weight, 0);

    m_noLoadResult->setText(
        QStringLiteral("空载损耗：%1 W").arg(QString::number(noLoadLoss, 'f', 0)));
}

void LossCalcPage::onCalcLoadLoss()
{
    const double hvR = m_hvResistance->value();
    const double lvR = m_lvResistance->value();
    const double hvI = m_hvCurrent->value();
    const double lvI = m_lvCurrent->value();
    const double hvExtra = m_hvExtraLoss->value();
    const double lvExtra = m_lvExtraLoss->value();
    const double strayPct = m_strayFactor->value();

    // 电阻损耗 I²R（3 相）
    const double hvCopperLoss = excelRound(3.0 * hvI * hvI * hvR, 0);
    const double lvCopperLoss = excelRound(3.0 * lvI * lvI * lvR, 0);

    // 负载损耗 = (ΣI²R + 附加损耗) × (1 + 杂散系数)
    const double strayFactor = strayPct / 100.0;
    const double loadLoss = excelRound(
        (hvCopperLoss + lvCopperLoss + hvExtra + lvExtra) * (1.0 + strayFactor), 0);

    m_loadLossResult->setText(
        QStringLiteral(
            "<table cellpadding='4'>"
            "<tr><td>高压电阻损耗:</td><td>%1 W</td></tr>"
            "<tr><td>低压电阻损耗:</td><td>%2 W</td></tr>"
            "<tr><td>附加损耗:</td><td>%3 W</td></tr>"
            "<tr><td>杂散系数:</td><td>%4 %</td></tr>"
            "<tr><td colspan='2'>&nbsp;</td></tr>"
            "<tr><td style='color:#4dd0e1; font-weight:bold;'>负载损耗:</td>"
            "<td style='color:#4dd0e1; font-size:16px; font-weight:bold;'>%5 W</td></tr>"
            "</table>")
            .arg(QString::number(hvCopperLoss, 'f', 0),
                 QString::number(lvCopperLoss, 'f', 0),
                 QString::number(hvExtra + lvExtra, 'f', 0),
                 QString::number(strayPct, 'f', 1),
                 QString::number(loadLoss, 'f', 0)));
}
