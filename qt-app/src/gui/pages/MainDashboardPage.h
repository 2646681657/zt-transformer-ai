#ifndef MAINDASHBOARDPAGE_H
#define MAINDASHBOARDPAGE_H
// 主仪表盘页（登录后首页，展示功能卡片入口）

#include <QWidget>
#include <QPushButton>

class QHBoxLayout;
class QVBoxLayout;
class QButtonGroup;
class QStackedWidget;

class MainDashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit MainDashboardPage(const QString &username, QWidget *parent = nullptr);

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
    QString m_username;
    QButtonGroup *m_navGroup;
    QStackedWidget *m_subStack;
};

#endif // MAINDASHBOARDPAGE_H
