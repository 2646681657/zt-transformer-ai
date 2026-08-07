#include "RibbonButton.h"
#include <QStyle>
#include <QIcon>

RibbonButton::RibbonButton(const QString &text, QWidget *parent)
    : QToolButton(parent)
{
    setText(text);
    init();
}

RibbonButton::RibbonButton(const QString &text, const QString &iconPath, QWidget *parent)
    : QToolButton(parent)
{
    setText(text);
    setIcon(QIcon(iconPath));
    setIconSize(QSize(36, 36));
    init();
}

void RibbonButton::init()
{
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setCheckable(true);
    setAutoExclusive(false);
    setMinimumWidth(84);
    setMinimumHeight(84);
    setObjectName("RibbonButton");
    connect(this, &QToolButton::clicked, this, &RibbonButton::onClicked);
}

void RibbonButton::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        setChecked(active);
        setProperty("active", active);
        style()->unpolish(this);
        style()->polish(this);
        emit activeChanged(active);
    }
}

void RibbonButton::onClicked()
{
    if (!isCheckable()) return;
    setActive(!m_active);
}
