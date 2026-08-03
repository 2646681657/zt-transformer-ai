#include "RibbonGroup.h"
#include "RibbonButton.h"

RibbonGroup::RibbonGroup(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 2, 6, 0);
    m_mainLayout->setSpacing(1);

    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setSpacing(2);
    m_mainLayout->addLayout(m_contentLayout, 1);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("RibbonGroupTitle");
    m_mainLayout->addWidget(m_titleLabel);

    setObjectName("RibbonGroup");
}

void RibbonGroup::addButton(RibbonButton *button)
{
    m_contentLayout->addWidget(button);
}

void RibbonGroup::addWidget(QWidget *widget)
{
    m_contentLayout->addWidget(widget);
}
