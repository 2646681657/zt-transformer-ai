#ifndef SIDEBARPANEL_H
#define SIDEBARPANEL_H
// 侧栏面板（垂直排列的快捷操作按钮列表）

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QVector>

class SidebarPanel : public QWidget {
    Q_OBJECT
public:
    explicit SidebarPanel(QWidget *parent = nullptr);
    QPushButton *addButton(const QString &text);
    // 带图标的侧栏按钮
    QPushButton *addButton(const QString &text, const QString &iconPath);

signals:
    void buttonClicked(int index);

private:
    QVBoxLayout *m_layout;
    QVector<QPushButton *> m_buttons;
};

#endif // SIDEBARPANEL_H
