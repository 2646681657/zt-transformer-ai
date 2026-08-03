#include "RibbonBar.h"
#include "RibbonGroup.h"
#include <QFrame>

RibbonBar::RibbonBar(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(0);
    m_layout->addStretch();
    setFixedHeight(70);
    setObjectName("RibbonBar");
}

RibbonGroup *RibbonBar::addGroup(const QString &title)
{
    int stretchIdx = m_layout->count() - 1;
    auto *group = new RibbonGroup(title, this);
    m_layout->insertWidget(stretchIdx, group);
    return group;
}

void RibbonBar::addSeparator()
{
    int stretchIdx = m_layout->count() - 1;
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setFixedWidth(1);
    sep->setObjectName("RibbonSeparator");
    m_layout->insertWidget(stretchIdx, sep);
}
