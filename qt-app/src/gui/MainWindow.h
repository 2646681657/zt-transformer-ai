#ifndef MAINWINDOW_H
#define MAINWINDOW_H
// 主窗口（管理页面栈切换：登录->仪表盘->参数设置->计算）

#include <QMainWindow>
#include <QStackedWidget>
#include "UserStore.h"
#include "StructureConfig.h"

class LoginPage;
class MainDashboardPage;
class OptimizeCalcPage;
class EnterCalcPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onLoginSuccess();
    void onLogout();
    void showDashboard();
    void showOptimizeCalc();
    void showEnterCalc();
    // 弹出类型选择对话框，确认后进入参数设置页
    void onOptimizeCalcRequested();

private:
    QStackedWidget *m_stack;
    UserStore m_userStore;
    StructureConfig m_config;
    LoginPage *m_loginPage;
    MainDashboardPage *m_dashboardPage;
    OptimizeCalcPage *m_optimizeCalcPage;
    EnterCalcPage *m_enterCalcPage;
};

#endif // MAINWINDOW_H
