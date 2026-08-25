#include "SchemePickDialog.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QDateTime>

SchemePickDialog::SchemePickDialog(const QString &title,
                                   const QVector<SchemeStore::SchemeEntry> &entries,
                                   const QString &storeFilePath, QWidget *parent)
    : QDialog(parent)
    , m_entries(entries)
    , m_storeFilePath(storeFilePath)
{
    setWindowTitle(title);
    setModal(true);
    resize(440, 380);

    auto *layout = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    fillList();
    layout->addWidget(m_list);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *delBtn = nullptr;
    if (!m_storeFilePath.isEmpty()) {
        delBtn = btns->addButton(QStringLiteral("删除所选"), QDialogButtonBox::ActionRole);
    }
    layout->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    if (delBtn) {
        connect(delBtn, &QPushButton::clicked, this, [this, btns]() {
            const int row = m_list->currentRow();
            if (row < 0 || row >= m_entries.size()) {
                return;
            }
            if (QMessageBox::question(this, QStringLiteral("删除方案"),
                    QStringLiteral("确定删除方案「%1」吗？").arg(m_entries[row].name))
                    != QMessageBox::Yes) {
                return;
            }
            m_entries.removeAt(row);
            SchemeStore::saveEntries(m_storeFilePath, m_entries);
            fillList();
            if (m_entries.isEmpty()) {
                btns->button(QDialogButtonBox::Ok)->setEnabled(false);
            }
        });
    }

    // 双击/确定取当前行；取消则 m_selected 保持默认（name 为空）
    connect(this, &QDialog::accepted, this, [this]() {
        const int row = m_list->currentRow();
        if (row >= 0 && row < m_entries.size()) {
            m_selected = m_entries[row];
        }
    });
}

// 列表填充：主行名称，副行时间（推荐方案无时间时显示描述已并入名称的场景则省略）
void SchemePickDialog::fillList()
{
    m_list->clear();
    for (const auto &e : m_entries) {
        m_list->addItem(QStringLiteral("%1    （%2）")
                            .arg(e.name,
                                 e.savedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    }
    m_list->setCurrentRow(m_entries.isEmpty() ? -1 : 0);
}
