#ifndef RIBBONBAR_H
#define RIBBONBAR_H
// Ribbon工具栏容器（水平排列多个分组，模拟Office风格选项卡）

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
