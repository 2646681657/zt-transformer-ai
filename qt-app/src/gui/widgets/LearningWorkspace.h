#ifndef LEARNINGWORKSPACE_H
#define LEARNINGWORKSPACE_H
#include "SelfLearningDataset.h"
#include <QWidget>

class QLabel;
class QLineEdit;
class QDateEdit;
class QPushButton;
class QTableWidget;
class LearningTrainingPanel;
class LearningPreparationPanel;

class LearningWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit LearningWorkspace(const QString &storagePath, QWidget *parent = nullptr);
    void setScheme(const CalcInput &input, const QString &name);
    QWidget *trainingPage() const { return m_trainingPage; }
    QWidget *preparationPage() const;
    bool confirmDiscard();
signals:
    void chooseScheme();
private:
    void reload();
    void refreshState();
    void showSample(const SelfLearning::Sample &sample);
    void newSample();
    void saveSample();
    void exportJson(bool training, bool selected);
    void importJson();
    void feedback(const QString &text, bool error = false);
    QString m_path, m_schemeName;
    CalcInput m_schemeInput;
    bool m_hasScheme = false, m_hasSample = false, m_dirty = false, m_loading = false, m_healthy = true;
    SelfLearning::Sample m_sample;
    QJsonObject m_original;
    QJsonObject m_readiness;
    QVector<SelfLearning::Sample> m_samples;
    QLabel *m_schemeLabel = nullptr, *m_feedback = nullptr, *m_trainingFeedback = nullptr, *m_trainingStatus = nullptr;
    QLineEdit *m_product = nullptr, *m_source = nullptr;
    QDateEdit *m_date = nullptr;
    std::array<QLineEdit *, SelfLearning::MetricCount> m_measured{};
    QTableWidget *m_list = nullptr, *m_values = nullptr, *m_counts = nullptr;
    QPushButton *m_new = nullptr, *m_save = nullptr, *m_template = nullptr, *m_exportTraining = nullptr;
    QWidget *m_trainingPage = nullptr;
    LearningTrainingPanel *m_trainingPanel = nullptr;
    LearningPreparationPanel *m_preparation = nullptr;
};
#endif
