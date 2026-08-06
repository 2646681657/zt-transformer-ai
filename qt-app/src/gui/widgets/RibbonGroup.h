#ifndef RIBBONGROUP_H
#define RIBBONGROUP_H
// Ribbon分组（一组相关按钮的容器，支持互斥选择模式）

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
    // 获取当前选中按钮的索引，无选中返回 -1
    int selectedIndex() const;
    // 获取按钮列表
    const QVector<RibbonButton *> &buttons() const { return m_buttons; }

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
