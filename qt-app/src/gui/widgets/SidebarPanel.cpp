#include "SidebarPanel.h"
#include <QIcon>

SidebarPanel::SidebarPanel(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 10, 6, 10);
    m_layout->setSpacing(8);
    m_layout->addStretch();
    setFixedWidth(130);
    setStyleSheet("SidebarPanel { background: #22262e; border-right: 1px solid #3a4050; }");
}

QPushButton *SidebarPanel::addButton(const QString &text)
{
    return addButton(text, QString());
}

QPushButton *SidebarPanel::addButton(const QString &text, const QString &iconPath)
{
    auto *btn = new QPushButton(text, this);
    if (!iconPath.isEmpty())
        btn->setIcon(QIcon(iconPath));
    btn->setFixedHeight(30);
    btn->setStyleSheet(
        "QPushButton { background: rgba(0,188,212,0.15); color: #4dd0e1;"
        "border: 1px solid #3a4050; border-radius: 4px; font-size: 11px; }"
        "QPushButton:hover { background: #00bcd4; color: #0d1117; border-color: #00bcd4; }");
    int idx = m_buttons.size();
    m_layout->insertWidget(idx, btn);
    m_buttons.append(btn);
    connect(btn, &QPushButton::clicked, this, [this, idx]() { emit buttonClicked(idx); });
    return btn;
}
