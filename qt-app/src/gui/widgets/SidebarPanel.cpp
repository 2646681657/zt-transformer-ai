#include "SidebarPanel.h"

SidebarPanel::SidebarPanel(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 10, 6, 10);
    m_layout->setSpacing(8);
    m_layout->addStretch();
    setFixedWidth(130);
    setStyleSheet("SidebarPanel { background: #f0f4f8; border-right: 1px solid #ddd; }");
}

QPushButton *SidebarPanel::addButton(const QString &text)
{
    auto *btn = new QPushButton(text, this);
    btn->setFixedHeight(30);
    btn->setStyleSheet(
        "QPushButton { background: #4a7aba; color: white; border-radius: 3px; font-size: 11px; }"
        "QPushButton:hover { background: #5a8aca; }");
    int idx = m_buttons.size();
    m_layout->insertWidget(idx, btn);
    m_buttons.append(btn);
    connect(btn, &QPushButton::clicked, this, [this, idx]() { emit buttonClicked(idx); });
    return btn;
}
