#ifndef RIBBONBUTTON_H
#define RIBBONBUTTON_H

#include <QToolButton>

class RibbonButton : public QToolButton {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
public:
    explicit RibbonButton(const QString &text, QWidget *parent = nullptr);

    bool isActive() const { return m_active; }
    void setActive(bool active);

signals:
    void activeChanged(bool active);

private:
    bool m_active = false;
};

#endif // RIBBONBUTTON_H
