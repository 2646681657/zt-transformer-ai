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
    m_buttons.append(button);
    connect(button, &QToolButton::toggled, this, &RibbonGroup::onButtonToggled);
}

void RibbonGroup::addWidget(QWidget *widget)
{
    m_contentLayout->addWidget(widget);
}

void RibbonGroup::setExclusive(bool exclusive)
{
    m_exclusive = exclusive;
}

bool RibbonGroup::hasSelection() const
{
    for (auto *btn : m_buttons) {
        if (btn->isChecked())
            return true;
    }
    return false;
}

void RibbonGroup::onButtonToggled(bool checked)
{
    if (!m_exclusive) return;

    auto *sender = qobject_cast<RibbonButton *>(this->sender());
    if (!sender) return;

    if (checked) {
        for (auto *btn : m_buttons) {
            if (btn != sender && btn->isChecked()) {
                btn->blockSignals(true);
                btn->setActive(false);
                btn->blockSignals(false);
            }
        }
    }
    emit selectionChanged();
}

int RibbonGroup::selectedIndex() const
{
    for (int i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i]->isChecked())
            return i;
    }
    return -1;
}
