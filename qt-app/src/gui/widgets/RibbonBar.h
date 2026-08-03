#ifndef RIBBONBAR_H
#define RIBBONBAR_H

#include <QWidget>
#include <QHBoxLayout>

class RibbonGroup;

class RibbonBar : public QWidget {
    Q_OBJECT
public:
    explicit RibbonBar(QWidget *parent = nullptr);
    RibbonGroup *addGroup(const QString &title);
    void addSeparator();

private:
    QHBoxLayout *m_layout;
};

#endif // RIBBONBAR_H
