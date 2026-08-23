#ifndef DATAQUERYPAGE_H
#define DATAQUERYPAGE_H
// 数据查询页（只读浏览设计基础数据库：硅钢性能曲线/性能标准值/
// 铁芯叠积对照表/线规表/波纹油箱系数，支持关键字过滤）

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QLineEdit;
class QLabel;

class DataQueryPage : public QWidget {
    Q_OBJECT
public:
    explicit DataQueryPage(QWidget *parent = nullptr);

private slots:
    void onTreeCurrentChanged(QTreeWidgetItem *current);
    void onSearchChanged(const QString &text);

private:
    void setupUi();
    void loadTree();
    // 按类别（与牌号序号）填充右侧表格
    void showTable(int category, int gradeIdx);
    // 按关键字隐藏不匹配行并刷新计数标签
    void applyFilter();

    QTreeWidget *m_tree = nullptr;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_countLabel = nullptr;
};

#endif // DATAQUERYPAGE_H
