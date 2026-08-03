#ifndef MAINDASHBOARDPAGE_H
#define MAINDASHBOARDPAGE_H

#include <QWidget>
#include <QPushButton>

class QHBoxLayout;
class QVBoxLayout;

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

private:
    void setupToolBar(QHBoxLayout *layout);
    void setupCardArea(QHBoxLayout *layout);
    QString m_username;
};

#endif // MAINDASHBOARDPAGE_H
