#ifndef SCHEMEPICKDIALOG_H
#define SCHEMEPICKDIALOG_H
// 通用方案选择对话框：列表展示方案条目（名称+时间/描述），双击或确定选中。
// 可选删除所选（传入存储路径时删除后自动写回文件）；推荐方案等只读列表不传路径即可

#include <QDialog>
#include <QVector>
#include "SchemeStore.h"

class QListWidget;

class SchemePickDialog : public QDialog {
    Q_OBJECT
public:
    // storeFilePath 非空时显示"删除所选"按钮，删除后写回该文件
    explicit SchemePickDialog(const QString &title,
                              const QVector<SchemeStore::SchemeEntry> &entries,
                              const QString &storeFilePath = QString(),
                              QWidget *parent = nullptr);

    // 选中条目（取消时 name 为空）
    SchemeStore::SchemeEntry selectedEntry() const { return m_selected; }
    // 是否有选中（exec() 返回 Accepted 且列表非空）
    bool hasSelection() const { return !m_selected.name.isEmpty(); }

private:
    void fillList();

    QVector<SchemeStore::SchemeEntry> m_entries;   // 删除后随之更新
    QString m_storeFilePath;                       // 非空：删除写回此文件
    SchemeStore::SchemeEntry m_selected;
    QListWidget *m_list;
};

#endif // SCHEMEPICKDIALOG_H
