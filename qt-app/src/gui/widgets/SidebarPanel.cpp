#include "SidebarPanel.h"
#include <QIcon>
#include <QSizePolicy>

SidebarPanel::SidebarPanel(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 10, 6, 10);
    m_layout->setSpacing(8);
    m_layout->addStretch();
    setFixedWidth(130);
    setStyleSheet("SidebarPanel { background: #FFFFFF; border-right: 1px solid #D3DDD6; }");
}

QToolButton *SidebarPanel::addButton(const QString &text)
{
    return addButton(text, QString());
}

QToolButton *SidebarPanel::addButton(const QString &text, const QString &iconPath)
{
    auto *btn = new QToolButton(this);
    btn->setText(text);
    if (!iconPath.isEmpty())
        btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(24, 24));
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setFixedHeight(64);
    btn->setStyleSheet(
        "QToolButton { background: rgba(33, 115, 70,0.15); color: #185C37;"
        "border: 1px solid #D3DDD6; border-radius: 4px; font-size: 11px; padding: 4px; }"
        "QToolButton:hover { background: #217346; color: #FFFFFF; border-color: #217346; }");
    int idx = m_buttons.size();
    m_layout->insertWidget(idx, btn);
    m_buttons.append(btn);
    connect(btn, &QToolButton::clicked, this, [this, idx]() { emit buttonClicked(idx); });
    return btn;
}
