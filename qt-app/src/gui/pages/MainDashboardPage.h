#ifndef MAINDASHBOARDPAGE_H
#define MAINDASHBOARDPAGE_H
// 主仪表盘页（登录后首页，展示功能卡片入口）

#include <QWidget>
#include <QPushButton>
#include "CalcResult.h"
#include "CalcInput.h"
#include "TransformerParams.h"

class QHBoxLayout;
class QVBoxLayout;
class QButtonGroup;
class QStackedWidget;
class DataQueryPage;
class QuotePage;
class ImpedanceCalcPage;
class LossCalcPage;
class DataManagementPage;
class UserManagementPage;
class SettingsPage;
class SwParametricDrawingPage;

class MainDashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit MainDashboardPage(const QString &username, QWidget *parent = nullptr);

    // 载入已确认方案到内嵌报价页（方案确认后由 MainWindow 调用）
    void loadQuoteScheme(const TransformerParams &params, const CalcInput &input,
                         const CalcResult &result);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void navigateToOptimizeCalc();
    void logoutRequested();

private slots:
    void onLogoutClicked();
    void onNavButtonClicked(int index);

private:
    void setupToolBar(QHBoxLayout *layout);
    void setupSubArea();
    QWidget *createOptimizeSubPage();
    QWidget *createToolsSubPage();
    QString m_username;
    QButtonGroup *m_navGroup;
    QStackedWidget *m_subStack;
    QWidget *m_subArea = nullptr;             // 子按钮区容器（功能页时整体隐藏）
    QStackedWidget *m_contentStack = nullptr; // 内容区（空白页/内嵌功能页）
    DataQueryPage *m_dataQueryPage = nullptr; // 内嵌数据查询页
    QuotePage *m_quotePage = nullptr;         // 内嵌产品报价页
    ImpedanceCalcPage *m_impedanceCalcPage = nullptr; // 阻抗计算器
    LossCalcPage *m_lossCalcPage = nullptr;             // 损耗计算器
    DataManagementPage *m_dataMgmtPage = nullptr;      // 数据管理
    UserManagementPage *m_userMgmtPage = nullptr;      // 用户管理
    SettingsPage *m_settingsPage = nullptr;            // 系统设置
    SwParametricDrawingPage *m_swDrawingPage = nullptr; // SW 参数化出图
};

#endif // MAINDASHBOARDPAGE_H
