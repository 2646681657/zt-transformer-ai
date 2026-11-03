#pragma once
#include "SelfLearningDataset.h"
#include <QWidget>
#include <functional>
class QComboBox;
class QTableWidget;
class QPushButton;
class QLabel;
class LearningPreparationPanel : public QWidget {
    Q_OBJECT
public:
    explicit LearningPreparationPanel(QString path,QWidget *parent=nullptr);
    void setDataset(const QVector<SelfLearning::Sample>&samples,bool healthy);
    void setBeforeMutation(std::function<bool()> guard) { m_guard=std::move(guard); }
signals:
    void datasetChanged();
private:
    void selectProduct();
    void assignPurpose();
    void exportCsv();
    void importCsv();
    void feedback(const QString &text,bool error=false);
    QString m_path;
    QVector<SelfLearning::Sample> m_samples;
    bool m_healthy=false;
    QComboBox *m_products=nullptr,*m_purpose=nullptr;
    QTableWidget *m_overview=nullptr,*m_issues=nullptr;
    QPushButton *m_assign=nullptr,*m_export=nullptr,*m_import=nullptr;
    QLabel *m_summary=nullptr,*m_feedback=nullptr;
    std::function<bool()> m_guard;
};
