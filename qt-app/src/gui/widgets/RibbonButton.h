#ifndef RIBBONBUTTON_H
#define RIBBONBUTTON_H

#include <QToolButton>

class RibbonButton : public QToolButton {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
public:
    explicit RibbonButton(const QString &text, QWidget *parent = nullptr);
    // 带图标的构造，iconPath 为 qrc 资源路径
    RibbonButton(const QString &text, const QString &iconPath, QWidget *parent = nullptr);

    bool isActive() const { return m_active; }
    void setActive(bool active);

signals:
    void activeChanged(bool active);

private slots:
    void onClicked();

private:
    void init();
    bool m_active = false;
};

#endif // RIBBONBUTTON_H
