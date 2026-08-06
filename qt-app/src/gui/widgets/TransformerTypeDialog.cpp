#include "TransformerTypeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QGroupBox>

TransformerTypeDialog::TransformerTypeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("选择变压器类型"));
    setFixedSize(420, 300);
    setStyleSheet("QDialog { background: #22262e; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(18);
    mainLayout->setContentsMargins(28, 24, 28, 24);

    // 标题
    auto *title = new QLabel(QStringLiteral("请选择计算类型"), this);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #4dd0e1;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    // 变压器类别
    auto *catBox = new QGroupBox(QStringLiteral("变压器类别"), this);
    auto *catLayout = new QHBoxLayout(catBox);
    m_categoryGroup = new QButtonGroup(this);

    auto *oilBtn = new QRadioButton(QStringLiteral("油浸式变压器（油变）"), catBox);
    auto *dryBtn = new QRadioButton(QStringLiteral("干式变压器（干变）"), catBox);
    oilBtn->setChecked(true);
    m_categoryGroup->addButton(oilBtn, 0);
    m_categoryGroup->addButton(dryBtn, 1);
    catLayout->addWidget(oilBtn);
    catLayout->addWidget(dryBtn);
    mainLayout->addWidget(catBox);

    // 绕组工艺
    auto *procBox = new QGroupBox(QStringLiteral("绕组工艺"), this);
    auto *procLayout = new QHBoxLayout(procBox);
    m_processGroup = new QButtonGroup(this);

    auto *foilBtn = new QRadioButton(QStringLiteral("箔绕"), procBox);
    auto *wireBtn = new QRadioButton(QStringLiteral("线绕"), procBox);
    foilBtn->setChecked(true);
    m_processGroup->addButton(foilBtn, 0);
    m_processGroup->addButton(wireBtn, 1);
    procLayout->addWidget(foilBtn);
    procLayout->addWidget(wireBtn);
    mainLayout->addWidget(procBox);

    // 按钮
    mainLayout->addStretch();
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    auto *okBtn = new QPushButton(QStringLiteral("确定"), this);
    okBtn->setDefault(true);
    okBtn->setStyleSheet("QPushButton { background: #00bcd4; color: #0d1117; "
                         "padding: 7px 28px; border-radius: 5px; font-weight: bold; }"
                         "QPushButton:hover { background: #4dd0e1; }");
    cancelBtn->setStyleSheet("QPushButton { padding: 7px 28px; border-radius: 5px;"
                             "background: #2a2f38; color: #8a9bb0; border: 1px solid #3a4050; }"
                             "QPushButton:hover { border-color: #00bcd4; color: #e0e6ed; }");
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, [this]() {
        m_category = m_categoryGroup->checkedId() == 0 ?
            StructureConfig::OilImmersed : StructureConfig::DryType;
        m_process = m_processGroup->checkedId() == 0 ?
            StructureConfig::FoilWound : StructureConfig::WireWound;
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}