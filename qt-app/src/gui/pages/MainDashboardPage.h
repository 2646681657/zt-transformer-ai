#ifndef MAINDASHBOARDPAGE_H
#define MAINDASHBOARDPAGE_H

#include <QWidget>
#include <QPushButton>

class MainDashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit MainDashboardPage(const QString &username, QWidget *parent = nullptr);

signals:
    void navigateToOptimizeCalc();
    void logoutRequested();

private slots:
    void onLogoutClicked();

private:
    void setupNavBar(QLayout *parent);
    void setupSubButtons(QLayout *parent);
    QString m_username;
};

#endif // MAINDASHBOARDPAGE_H
