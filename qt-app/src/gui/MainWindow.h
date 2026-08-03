#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "UserStore.h"

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

private:
    QStackedWidget *m_stack;
    UserStore m_userStore;
    LoginPage *m_loginPage;
    MainDashboardPage *m_dashboardPage;
    OptimizeCalcPage *m_optimizeCalcPage;
    EnterCalcPage *m_enterCalcPage;
};

#endif // MAINWINDOW_H
