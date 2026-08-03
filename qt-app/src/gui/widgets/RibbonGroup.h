#ifndef RIBBONGROUP_H
#define RIBBONGROUP_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class RibbonButton;

class RibbonGroup : public QWidget {
    Q_OBJECT
public:
    explicit RibbonGroup(const QString &title, QWidget *parent = nullptr);
    void addButton(RibbonButton *button);
    void addWidget(QWidget *widget);
    QHBoxLayout *contentLayout() { return m_contentLayout; }

private:
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_contentLayout;
    QLabel *m_titleLabel;
};

#endif // RIBBONGROUP_H
