#include "RibbonButton.h"
#include <QStyle>

RibbonButton::RibbonButton(const QString &text, QWidget *parent)
    : QToolButton(parent)
{
    setText(text);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setCheckable(true);
    setAutoExclusive(false);
    setMinimumWidth(50);
    setObjectName("RibbonButton");
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
