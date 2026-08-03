#include "MainWindow.h"
#include "pages/LoginPage.h"
#include "pages/MainDashboardPage.h"
#include "pages/OptimizeCalcPage.h"
#include "pages/EnterCalcPage.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("同优计算优化设计软件(V2.0)"));
    resize(1280, 800);

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_loginPage = new LoginPage(&m_userStore, this);
    m_dashboardPage = new MainDashboardPage(QString(), this);
    m_optimizeCalcPage = new OptimizeCalcPage(this);
    m_enterCalcPage = new EnterCalcPage(this);

    m_stack->addWidget(m_loginPage);
    m_stack->addWidget(m_dashboardPage);
    m_stack->addWidget(m_optimizeCalcPage);
    m_stack->addWidget(m_enterCalcPage);

    connect(m_loginPage, &LoginPage::loginSuccess, this, &MainWindow::onLoginSuccess);
    connect(m_dashboardPage, &MainDashboardPage::navigateToOptimizeCalc, this, &MainWindow::showOptimizeCalc);
    connect(m_dashboardPage, &MainDashboardPage::logoutRequested, this, &MainWindow::onLogout);
    connect(m_optimizeCalcPage, &OptimizeCalcPage::navigateToEnterCalc, this, &MainWindow::showEnterCalc);
    connect(m_optimizeCalcPage, &OptimizeCalcPage::navigateBack, this, &MainWindow::showDashboard);
    connect(m_enterCalcPage, &EnterCalcPage::navigateBack, this, &MainWindow::showOptimizeCalc);

    m_stack->setCurrentWidget(m_loginPage);
}

void MainWindow::onLoginSuccess()
{
    m_stack->setCurrentWidget(m_dashboardPage);
}

void MainWindow::onLogout()
{
    m_userStore.logout();
    m_stack->setCurrentWidget(m_loginPage);
}

void MainWindow::showDashboard()
{
    m_stack->setCurrentWidget(m_dashboardPage);
}

void MainWindow::showOptimizeCalc()
{
    m_stack->setCurrentWidget(m_optimizeCalcPage);
}

void MainWindow::showEnterCalc()
{
    m_enterCalcPage->setParams(m_optimizeCalcPage->currentParams());
    m_stack->setCurrentWidget(m_enterCalcPage);
}
