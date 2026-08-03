#ifndef RIBBONGROUP_H
#define RIBBONGROUP_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVector>

class RibbonButton;

class RibbonGroup : public QWidget {
    Q_OBJECT
public:
    explicit RibbonGroup(const QString &title, QWidget *parent = nullptr);
    void addButton(RibbonButton *button);
    void addWidget(QWidget *widget);
    QHBoxLayout *contentLayout() { return m_contentLayout; }
    // 设置组内按钮互斥模式
    void setExclusive(bool exclusive);
    // 当前是否有选中项
    bool hasSelection() const;

signals:
    void selectionChanged();

private slots:
    void onButtonToggled(bool checked);

private:
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_contentLayout;
    QLabel *m_titleLabel;
    QVector<RibbonButton *> m_buttons;
    bool m_exclusive = false;
};

#endif // RIBBONGROUP_H
