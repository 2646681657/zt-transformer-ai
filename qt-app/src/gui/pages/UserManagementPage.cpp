#include "UserManagementPage.h"
#include "UserStore.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QIntValidator>

UserManagementPage::UserManagementPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshTable();
}

void UserManagementPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 工具栏
    auto *toolbar = new QWidget(this);
    toolbar->setFixedHeight(34);
    toolbar->setStyleSheet("background: #2a2f38; border-bottom: 1px solid #3a4050;");
    auto *toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(8, 4, 8, 4);
    auto *pageLabel = new QLabel(QStringLiteral("用户管理"), toolbar);
    pageLabel->setStyleSheet("color: #4dd0e1; font-size: 12px; font-weight: bold;");
    toolLayout->addWidget(pageLabel);
    toolLayout->addStretch();

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), toolbar);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(
        "QPushButton { color: #8a9bb0; font-size: 11px; padding: 3px 12px;"
        " border: 1px solid #3a4050; border-radius: 3px; background: #2a2f38; }"
        "QPushButton:hover { border-color: #00bcd4; color: #4dd0e1; }");
    toolLayout->addWidget(refreshBtn);
    connect(refreshBtn, &QPushButton::clicked, this, &UserManagementPage::onRefresh);
    mainLayout->addWidget(toolbar);

    // 内容区
    auto *content = new QWidget(this);
    content->setStyleSheet("background: #1a1d23;");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(12);

    // 权限提示
    m_permLabel = new QLabel(content);
    m_permLabel->setStyleSheet("color: #c0c8d0; font-size: 12px; padding: 8px 12px;"
                               " background: #22262e; border-radius: 4px;");
    contentLayout->addWidget(m_permLabel);

    // 用户列表表格
    m_table = new QTableWidget(content);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("用户名"), QStringLiteral("角色"), QStringLiteral("显示名称")});
    m_table->setStyleSheet(
        "QTableWidget { background: #22262e; alternate-background-color: #262b34;"
        " color: #e0e6ed; gridline-color: #3a4050; border: 1px solid #3a4050; }"
        "QHeaderView::section { background: #2a2f38; color: #4dd0e1;"
        " padding: 6px; border: none; border-bottom: 1px solid #3a4050; font-weight: bold; }"
        "QTableWidget::item { padding: 6px 8px; }"
        "QTableWidget::item:selected { background: rgba(0,188,212,0.25); color: #e0e6ed; }");
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setMinimumHeight(280);
    contentLayout->addWidget(m_table, 1);

    // 操作按钮组
    auto makeBtn = [](const QString &text, const QString &color = "#00bcd4") {
        auto *btn = new QPushButton(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton { background: %1; color: #1a1d23; font-size: 12px;"
                    " padding: 8px 20px; border: none; border-radius: 4px; font-weight: bold; }"
                    "QPushButton:hover { background: %2; }"
                    "QPushButton:disabled { background: #3a4050; color: #5a6070; }")
                .arg(color, color == "#00bcd4" ? "#4dd0e1" :
                            color == "#ef5350" ? "#ef9a9a" :
                            color == "#ff9800" ? "#ffb74d" : "#81c784"));
        btn->setFixedWidth(140);
        return btn;
    };

    auto *btnGroup = new QGroupBox(content);
    btnGroup->setStyleSheet(
        "QGroupBox { color: #c0c8d0; font-size: 12px; border: 1px solid #3a4050;"
        " border-radius: 4px; margin-top: 10px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }");
    auto *btnLayout = new QHBoxLayout(btnGroup);
    btnLayout->setContentsMargins(12, 8, 12, 12);
    btnLayout->setSpacing(12);

    auto *addBtn = makeBtn(QStringLiteral("添加用户"), "#4caf50");
    auto *delBtn = makeBtn(QStringLiteral("删除用户"), "#ef5350");
    auto *roleBtn = makeBtn(QStringLiteral("修改角色"), "#ff9800");
    auto *pwdBtn = makeBtn(QStringLiteral("修改密码"), "#2196f3");

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(delBtn);
    btnLayout->addWidget(roleBtn);
    btnLayout->addWidget(pwdBtn);
    btnLayout->addStretch();
    contentLayout->addWidget(btnGroup);

    // 缓存按钮指针用于权限控制
    m_addBtn = addBtn;
    m_delBtn = delBtn;
    m_roleBtn = roleBtn;
    m_pwdBtn = pwdBtn;

    connect(addBtn, &QPushButton::clicked, this, &UserManagementPage::onAddUser);
    connect(delBtn, &QPushButton::clicked, this, &UserManagementPage::onDeleteUser);
    connect(roleBtn, &QPushButton::clicked, this, &UserManagementPage::onChangeRole);
    connect(pwdBtn, &QPushButton::clicked, this, &UserManagementPage::onChangePassword);

    contentLayout->addStretch();
    mainLayout->addWidget(content, 1);
}

void UserManagementPage::refreshTable()
{
    auto &store = UserStore::instance();
    const bool isAdmin = store.isAdmin();
    const QString currentUser = store.currentUser();

    // 更新权限提示
    if (isAdmin) {
        m_permLabel->setText(QStringLiteral("当前用户：%1 （管理员）— 可添加/删除用户、修改角色与密码")
            .arg(currentUser));
        m_permLabel->setStyleSheet("color: #4dd0e1; font-size: 12px; padding: 8px 12px;"
                                   " background: #22262e; border-radius: 4px;");
    } else {
        m_permLabel->setText(QStringLiteral("当前用户：%1 （普通用户）— 仅可修改自己的密码")
            .arg(currentUser));
        m_permLabel->setStyleSheet("color: #c0c8d0; font-size: 12px; padding: 8px 12px;"
                                   " background: #22262e; border-radius: 4px;");
    }

    // 普通用户禁用管理类按钮
    m_addBtn->setEnabled(isAdmin);
    m_delBtn->setEnabled(isAdmin);
    m_roleBtn->setEnabled(isAdmin);

    // 填充表格
    const QVector<UserInfo> users = store.users();
    m_table->setRowCount(users.size());
    for (int i = 0; i < users.size(); ++i) {
        const auto &u = users[i];
        auto *nameItem = new QTableWidgetItem(u.username);
        nameItem->setData(Qt::UserRole, u.username);
        // 当前用户行高亮
        if (u.username == currentUser) {
            nameItem->setBackground(QColor(0, 188, 212, 40));
        }

        QString roleText = (u.role == "admin") ? QStringLiteral("管理员") : QStringLiteral("普通用户");
        auto *roleItem = new QTableWidgetItem(roleText);
        roleItem->setData(Qt::UserRole, u.role);
        if (u.username == currentUser) {
            roleItem->setBackground(QColor(0, 188, 212, 40));
        }

        auto *dispItem = new QTableWidgetItem(u.displayName);
        if (u.username == currentUser) {
            dispItem->setBackground(QColor(0, 188, 212, 40));
        }

        m_table->setItem(i, 0, nameItem);
        m_table->setItem(i, 1, roleItem);
        m_table->setItem(i, 2, dispItem);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);
}

void UserManagementPage::onRefresh()
{
    refreshTable();
}

void UserManagementPage::onAddUser()
{
    if (!UserStore::instance().isAdmin()) {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
            QStringLiteral("仅管理员可添加用户"));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("添加用户"));
    dlg.setStyleSheet("QDialog { background: #1a1d23; }"
                      "QLabel { color: #c0c8d0; font-size: 12px; }"
                      "QLineEdit, QComboBox { background: #22262e; color: #e0e6ed;"
                      " border: 1px solid #3a4050; border-radius: 4px; padding: 6px 8px; }"
                      "QComboBox QAbstractItemView { background: #22262e; color: #e0e6ed;"
                      " selection-background-color: rgba(0,188,212,0.3); }");

    auto *form = new QFormLayout(&dlg);
    form->setContentsMargins(20, 20, 20, 12);
    form->setSpacing(10);

    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(QStringLiteral("登录用户名"));
    form->addRow(QStringLiteral("用户名："), nameEdit);

    auto *pwdEdit = new QLineEdit(&dlg);
    pwdEdit->setPlaceholderText(QStringLiteral("初始密码"));
    pwdEdit->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("密码："), pwdEdit);

    auto *roleCombo = new QComboBox(&dlg);
    roleCombo->addItem(QStringLiteral("普通用户"), "user");
    roleCombo->addItem(QStringLiteral("管理员"), "admin");
    form->addRow(QStringLiteral("角色："), roleCombo);

    auto *dispEdit = new QLineEdit(&dlg);
    dispEdit->setPlaceholderText(QStringLiteral("可选，如：张三"));
    form->addRow(QStringLiteral("显示名称："), dispEdit);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    btns->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    form->addRow(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = nameEdit->text().trimmed();
    const QString pwd = pwdEdit->text();
    const QString role = roleCombo->currentData().toString();
    const QString disp = dispEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入错误"), QStringLiteral("用户名不能为空"));
        return;
    }
    if (pwd.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入错误"), QStringLiteral("密码不能为空"));
        return;
    }

    if (UserStore::instance().addUser(name, pwd, role, disp)) {
        refreshTable();
        QMessageBox::information(this, QStringLiteral("添加成功"),
            QStringLiteral("用户 %1 已添加").arg(name));
    } else {
        QMessageBox::warning(this, QStringLiteral("添加失败"),
            UserStore::instance().lastError());
    }
}

void UserManagementPage::onDeleteUser()
{
    if (!UserStore::instance().isAdmin()) {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
            QStringLiteral("仅管理员可删除用户"));
        return;
    }

    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("未选中用户"),
            QStringLiteral("请先在表格中选择要删除的用户"));
        return;
    }

    const QString name = m_table->item(row, 0)->text();
    auto ret = QMessageBox::question(this, QStringLiteral("确认删除"),
        QStringLiteral("确定要删除用户 %1 吗？").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    if (UserStore::instance().deleteUser(name)) {
        refreshTable();
        QMessageBox::information(this, QStringLiteral("删除成功"),
            QStringLiteral("用户 %1 已删除").arg(name));
    } else {
        QMessageBox::warning(this, QStringLiteral("删除失败"),
            UserStore::instance().lastError());
    }
}

void UserManagementPage::onChangePassword()
{
    int row = m_table->currentRow();
    QString target;
    if (row >= 0) {
        target = m_table->item(row, 0)->text();
    } else {
        // 未选中则默认修改当前用户密码
        target = UserStore::instance().currentUser();
    }

    // 普通用户只能改自己的密码
    const bool isAdmin = UserStore::instance().isAdmin();
    if (!isAdmin && target != UserStore::instance().currentUser()) {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
            QStringLiteral("普通用户仅可修改自己的密码"));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("修改密码 - %1").arg(target));
    dlg.setStyleSheet("QDialog { background: #1a1d23; }"
                      "QLabel { color: #c0c8d0; font-size: 12px; }"
                      "QLineEdit { background: #22262e; color: #e0e6ed;"
                      " border: 1px solid #3a4050; border-radius: 4px; padding: 6px 8px; }");

    auto *form = new QFormLayout(&dlg);
    form->setContentsMargins(20, 20, 20, 12);
    form->setSpacing(10);

    auto *pwdEdit = new QLineEdit(&dlg);
    pwdEdit->setEchoMode(QLineEdit::Password);
    pwdEdit->setPlaceholderText(QStringLiteral("新密码"));
    form->addRow(QStringLiteral("新密码："), pwdEdit);

    auto *confirmEdit = new QLineEdit(&dlg);
    confirmEdit->setEchoMode(QLineEdit::Password);
    confirmEdit->setPlaceholderText(QStringLiteral("再次输入"));
    form->addRow(QStringLiteral("确认密码："), confirmEdit);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    btns->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    form->addRow(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString pwd = pwdEdit->text();
    const QString confirm = confirmEdit->text();
    if (pwd.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入错误"), QStringLiteral("密码不能为空"));
        return;
    }
    if (pwd != confirm) {
        QMessageBox::warning(this, QStringLiteral("输入错误"),
            QStringLiteral("两次输入的密码不一致"));
        return;
    }

    if (UserStore::instance().changePassword(target, pwd)) {
        QMessageBox::information(this, QStringLiteral("修改成功"),
            QStringLiteral("用户 %1 的密码已更新").arg(target));
    } else {
        QMessageBox::warning(this, QStringLiteral("修改失败"),
            UserStore::instance().lastError());
    }
}

void UserManagementPage::onChangeRole()
{
    if (!UserStore::instance().isAdmin()) {
        QMessageBox::warning(this, QStringLiteral("权限不足"),
            QStringLiteral("仅管理员可修改用户角色"));
        return;
    }

    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("未选中用户"),
            QStringLiteral("请先在表格中选择要修改的用户"));
        return;
    }

    const QString name = m_table->item(row, 0)->text();
    const QString curRole = m_table->item(row, 1)->data(Qt::UserRole).toString();
    const QString curDisp = m_table->item(row, 2)->text();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("修改角色 - %1").arg(name));
    dlg.setStyleSheet("QDialog { background: #1a1d23; }"
                      "QLabel { color: #c0c8d0; font-size: 12px; }"
                      "QLineEdit, QComboBox { background: #22262e; color: #e0e6ed;"
                      " border: 1px solid #3a4050; border-radius: 4px; padding: 6px 8px; }"
                      "QComboBox QAbstractItemView { background: #22262e; color: #e0e6ed;"
                      " selection-background-color: rgba(0,188,212,0.3); }");

    auto *form = new QFormLayout(&dlg);
    form->setContentsMargins(20, 20, 20, 12);
    form->setSpacing(10);

    auto *roleCombo = new QComboBox(&dlg);
    roleCombo->addItem(QStringLiteral("普通用户"), "user");
    roleCombo->addItem(QStringLiteral("管理员"), "admin");
    roleCombo->setCurrentIndex(curRole == "admin" ? 1 : 0);
    form->addRow(QStringLiteral("角色："), roleCombo);

    auto *dispEdit = new QLineEdit(&dlg);
    dispEdit->setText(curDisp);
    form->addRow(QStringLiteral("显示名称："), dispEdit);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    btns->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    form->addRow(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString newRole = roleCombo->currentData().toString();
    const QString newDisp = dispEdit->text().trimmed();

    if (UserStore::instance().updateUser(name, newRole, newDisp)) {
        refreshTable();
        QMessageBox::information(this, QStringLiteral("修改成功"),
            QStringLiteral("用户 %1 的信息已更新").arg(name));
    } else {
        QMessageBox::warning(this, QStringLiteral("修改失败"),
            UserStore::instance().lastError());
    }
}
