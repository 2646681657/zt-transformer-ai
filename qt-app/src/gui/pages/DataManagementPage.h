#ifndef DATAMANAGEMENTPAGE_H
#define DATAMANAGEMENTPAGE_H
// 数据管理页（程序工具子页）：设计数据导出/导入 + 用户数据备份/恢复

#include <QWidget>

class QLabel;
class QTableWidget;

class DataManagementPage : public QWidget {
    Q_OBJECT
public:
    explicit DataManagementPage(QWidget *parent = nullptr);

private slots:
    // 设计数据导出
    void onExportJson();
    void onExportCsv();
    // 设计数据导入
    void onImportJson();
    // 用户数据备份/恢复
    void onBackupUserData();
    void onRestoreUserData();

private:
    void setupUi();
    void refreshStats();

    QLabel *m_statsLabel = nullptr;   // 数据统计信息
};

#endif // DATAMANAGEMENTPAGE_H
