#ifndef SIDEBARPANEL_H
#define SIDEBARPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QVector>

class SidebarPanel : public QWidget {
    Q_OBJECT
public:
    explicit SidebarPanel(QWidget *parent = nullptr);
    QPushButton *addButton(const QString &text);

signals:
    void buttonClicked(int index);

private:
    QVBoxLayout *m_layout;
    QVector<QPushButton *> m_buttons;
};

#endif // SIDEBARPANEL_H
