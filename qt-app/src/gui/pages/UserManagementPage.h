#ifndef USERMANAGEMENTPAGE_H
#define USERMANAGEMENTPAGE_H
// 用户管理页（程序工具子页）：管理员增删用户/改密码/改角色，普通用户改自己密码

#include <QWidget>

class QTableWidget;
class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;

class UserManagementPage : public QWidget {
    Q_OBJECT
public:
    explicit UserManagementPage(QWidget *parent = nullptr);

protected:
    // 每次显示时刷新（页面创建早于登录，需在显示时重读登录状态）
    void showEvent(QShowEvent *event) override;

private slots:
    void onAddUser();
    void onDeleteUser();
    void onChangePassword();
    void onChangeRole();
    void onRefresh();

private:
    void setupUi();
    void refreshTable();

    QTableWidget *m_table = nullptr;
    QLabel *m_permLabel = nullptr;      // 权限提示
    QPushButton *m_addBtn = nullptr;    // 添加用户（管理员）
    QPushButton *m_delBtn = nullptr;     // 删除用户（管理员）
    QPushButton *m_roleBtn = nullptr;   // 修改角色（管理员）
    QPushButton *m_pwdBtn = nullptr;    // 修改密码（全部用户）
};

#endif // USERMANAGEMENTPAGE_H
