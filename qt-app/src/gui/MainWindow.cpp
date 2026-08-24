#include "MainWindow.h"
#include "pages/LoginPage.h"
#include "pages/MainDashboardPage.h"
#include "pages/OptimizeCalcPage.h"
#include "pages/EnterCalcPage.h"
#include "widgets/TransformerTypeDialog.h"
#include <QScreen>
#include <QGuiApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("中天伯乐达变压器电磁计算AI寻优软件 V2.0"));

    QScreen *screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen->availableSize();
    int w = qMin(screenSize.width() * 3 / 4, 1280);
    int h = qMin(screenSize.height() * 3 / 4, 800);
    resize(w, h);
    move((screenSize.width() - w) / 2, (screenSize.height() - h) / 2);

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_loginPage = new LoginPage(&UserStore::instance(), this);
    m_dashboardPage = new MainDashboardPage(QString(), this);
    m_optimizeCalcPage = new OptimizeCalcPage(this);
    m_enterCalcPage = new EnterCalcPage(this);

    m_stack->addWidget(m_loginPage);
    m_stack->addWidget(m_dashboardPage);
    m_stack->addWidget(m_optimizeCalcPage);
    m_stack->addWidget(m_enterCalcPage);

    connect(m_loginPage, &LoginPage::loginSuccess, this, &MainWindow::onLoginSuccess);
    connect(m_dashboardPage, &MainDashboardPage::navigateToOptimizeCalc,
            this, &MainWindow::onOptimizeCalcRequested);
    connect(m_dashboardPage, &MainDashboardPage::logoutRequested, this, &MainWindow::onLogout);
    connect(m_optimizeCalcPage, &OptimizeCalcPage::navigateToEnterCalc, this, &MainWindow::showEnterCalc);
    connect(m_optimizeCalcPage, &OptimizeCalcPage::navigateBack, this, &MainWindow::showDashboard);
    connect(m_enterCalcPage, &EnterCalcPage::navigateBack, this, &MainWindow::showOptimizeCalc);
    // 方案确认后同步数据到主界面内嵌产品报价页
    connect(m_enterCalcPage, &EnterCalcPage::schemeConfirmed,
            m_dashboardPage, &MainDashboardPage::loadQuoteScheme);

    m_stack->setCurrentWidget(m_loginPage);
}

void MainWindow::onLoginSuccess()
{
    m_stack->setCurrentWidget(m_dashboardPage);
}

void MainWindow::onLogout()
{
    UserStore::instance().logout();
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

// 弹出变压器类型对话框，用户确认后将选型传入参数设置页并切换过去
void MainWindow::onOptimizeCalcRequested()
{
    TransformerTypeDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_config.category = dlg.selectedCategory();
        m_config.windingProcess = dlg.selectedProcess();
        m_optimizeCalcPage->setStructureConfig(m_config);
        m_stack->setCurrentWidget(m_optimizeCalcPage);
    }
}

void MainWindow::showEnterCalc()
{
    m_enterCalcPage->setParams(m_optimizeCalcPage->currentParams());
    m_enterCalcPage->setCalcInput(m_optimizeCalcPage->currentInput());
    m_stack->setCurrentWidget(m_enterCalcPage);
}
