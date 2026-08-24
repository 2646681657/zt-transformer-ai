#include "ImpedanceCalcPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QPalette>
#include <QFont>
#include <QLineEdit>
#include <cmath>

namespace {
// 与 ElectromagneticEngine 一致的 excelRound
double excelRound(double v, int digits)
{
    const double p = std::pow(10.0, digits);
    return std::floor(v * p + 0.5) / p;
}
} // namespace

ImpedanceCalcPage::ImpedanceCalcPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ImpedanceCalcPage::setupUi()
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
    auto *pageLabel = new QLabel(QStringLiteral("阻抗计算器"), toolbar);
    pageLabel->setStyleSheet("color: #4dd0e1; font-size: 12px; font-weight: bold;");
    toolLayout->addWidget(pageLabel);
    toolLayout->addStretch();
    mainLayout->addWidget(toolbar);

    // 内容区
    auto *content = new QWidget(this);
    content->setStyleSheet("background: #1a1d23;");
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(16);

    // 左侧：参数输入
    auto *paramGroup = new QGroupBox(QStringLiteral("输入参数"), content);
    paramGroup->setStyleSheet(
        "QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
        " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    auto *form = new QFormLayout(paramGroup);
    form->setContentsMargins(12, 8, 12, 12);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    // 创建带深色样式的 spinbox 工厂
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

    m_capacity   = makeSpin(1.0, 100000.0, 630.0,  10.0, QStringLiteral(" kVA"));
    m_lvTurns    = makeSpin(1.0, 1000.0,    18.0,   1.0, QStringLiteral(""));
    m_lvCurrent  = makeSpin(0.1, 100000.0, 909.3,   1.0, QStringLiteral(" A"));
    m_turnVoltage = makeSpin(0.01, 100.0,   8.332,  0.1, QStringLiteral(" V"));
    m_lambda     = makeSpin(0.0, 500.0,     33.5,   0.5, QStringLiteral(" mm"));
    m_hx         = makeSpin(0.0, 1000.0,   340.9,   1.0, QStringLiteral(" mm"));
    m_leakArea   = makeSpin(0.0, 100000.0, 6000.0, 10.0, QStringLiteral(" mm²"));
    m_loadLoss   = makeSpin(0.0, 100000.0, 5206.0, 10.0, QStringLiteral(" W"));

    form->addRow(QStringLiteral("容量 (kVA)"), m_capacity);
    form->addRow(QStringLiteral("低压匝数"), m_lvTurns);
    form->addRow(QStringLiteral("低压相电流 (A)"), m_lvCurrent);
    form->addRow(QStringLiteral("匝电压 (V)"), m_turnVoltage);
    form->addRow(QStringLiteral("漏磁通道总厚 λ (mm)"), m_lambda);
    form->addRow(QStringLiteral("电抗高 hx (mm)"), m_hx);
    form->addRow(QStringLiteral("漏磁面积 (mm²)"), m_leakArea);
    form->addRow(QStringLiteral("负载损耗 (W)"), m_loadLoss);

    auto *calcBtn = new QPushButton(QStringLiteral("计算"), paramGroup);
    calcBtn->setCursor(Qt::PointingHandCursor);
    calcBtn->setStyleSheet(
        "QPushButton { background: #00bcd4; color: #1a1d23; font-size: 12px;"
        " padding: 6px 24px; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #4dd0e1; }");
    form->addRow(QString(), calcBtn);
    connect(calcBtn, &QPushButton::clicked, this, &ImpedanceCalcPage::onCalc);

    contentLayout->addWidget(paramGroup, 0, Qt::AlignTop);
    contentLayout->addStretch();

    // 右侧：结果区
    auto *resultGroup = new QGroupBox(QStringLiteral("计算结果"), content);
    resultGroup->setStyleSheet(paramGroup->styleSheet());
    auto *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setContentsMargins(12, 8, 12, 12);
    resultLayout->setSpacing(12);

    m_resultLabel = new QLabel(QStringLiteral("阻抗电压：—"), resultGroup);
    m_resultLabel->setStyleSheet("color: #4dd0e1; font-size: 20px; font-weight: bold;");
    resultLayout->addWidget(m_resultLabel);

    m_detailLabel = new QLabel(resultGroup);
    m_detailLabel->setStyleSheet("color: #c0c8d0; font-size: 12px;");
    m_detailLabel->setTextFormat(Qt::RichText);
    m_detailLabel->setWordWrap(true);
    resultLayout->addWidget(m_detailLabel);
    resultLayout->addStretch();

    contentLayout->addWidget(resultGroup, 1);

    mainLayout->addWidget(content, 1);
}

void ImpedanceCalcPage::onCalc()
{
    const double capacity = m_capacity->value();
    const double lvTurns = m_lvTurns->value();
    const double lvCurrent = m_lvCurrent->value();
    const double turnVoltage = m_turnVoltage->value();
    const double lambda = m_lambda->value();
    const double hx = m_hx->value();
    const double leakArea = m_leakArea->value();
    const double loadLoss = m_loadLoss->value();

    if (turnVoltage <= 0 || hx <= 0 || capacity <= 0) {
        m_resultLabel->setText(QStringLiteral("阻抗电压：输入参数有误"));
        return;
    }

    // 横向漏磁系数 Kx（Q32）
    // 在简化工具中直接取用户输入的 lambda/hx；
    // 当 lvFoilWidth ≈ ac30 时 Kx≈1，否则按实际尺寸修正
    // 这里取 Kx=1.0（标准情况），用户可在完整计算中修正
    const double kx = 1.0;

    // 电抗压降 P43（%）
    const double p43 = excelRound(
        kx * 3.95 * lvCurrent * lvTurns * leakArea
            / turnVoltage / hx / 1e5, 2);

    // 电阻压降 Q34（%）
    const double q34 = excelRound(loadLoss / capacity / 10.0, 2);

    // 阻抗电压 Q35（%）
    const double q35 = excelRound(std::sqrt(q34 * q34 + p43 * p43), 2);

    m_resultLabel->setText(
        QStringLiteral("阻抗电压：%1 %").arg(QString::number(q35, 'f', 2)));

    m_detailLabel->setText(
        QStringLiteral(
            "<table cellpadding='4'>"
            "<tr><td>电抗压降 P43:</td><td>%1 %</td></tr>"
            "<tr><td>电阻压降 Q34:</td><td>%2 %</td></tr>"
            "<tr><td>横向漏磁系数 Kx:</td><td>%3</td></tr>"
            "<tr><td colspan='2' style='color:#8a9bb0;'>阻抗 = √(电抗² + 电阻²)</td></tr>"
            "</table>")
            .arg(QString::number(p43, 'f', 2),
                 QString::number(q34, 'f', 2),
                 QString::number(kx, 'f', 2)));
}
