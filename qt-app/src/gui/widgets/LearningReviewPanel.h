#ifndef LEARNINGREVIEWPANEL_H
#define LEARNINGREVIEWPANEL_H
#include "SelfLearningDataset.h"
#include <QWidget>
#include <QProcess>
class QListWidget;
class QTableWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;

// Independent evaluation and local approval affect the display column only.
class LearningReviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit LearningReviewPanel(const QString &directory, QWidget *parent = nullptr);
    ~LearningReviewPanel() override;
    void setDataset(const QVector<SelfLearning::Sample> &samples);
    void setDatasetSource(const QString &path) { if(!isBusy()) m_datasetPath=path; }
    void setModels(const QStringList &models, const QString &selected);
    void setRuntime(const QString &python, const QString &script, bool ready, bool testOnly);
    bool isBusy() const { return m_process.state() != QProcess::NotRunning; }
    void requestCancel();
signals:
    void registryChanged();
    void busyChanged();
private:
    void evaluate();
    void loadRuns();
    void displayRun();
    void savePolicy();
    void reloadPolicy();
    QJsonObject editorPolicy() const;
    bool policyVisibleAndCurrent(QString &reason) const;
    void refresh();
    void finish(int code, QProcess::ExitStatus status);
    void consume();
    QString reviewDirectory() const { return m_directory + "/review"; }
    QString m_directory, m_selected, m_python, m_script, m_run, m_evaluationDirectory, m_datasetPath;
    bool m_ready = false, m_testOnly = false, m_cancelled = false, m_loading = false;
    quint64 m_generation = 0;
    QVector<SelfLearning::Sample> m_samples;
    QJsonObject m_displayedPolicy;
    QProcess m_process;
    QByteArray m_output, m_errors;
    QListWidget *m_sampleList, *m_models;
    QComboBox *m_runs;
    QTableWidget *m_results, *m_policy;
    QLineEdit *m_policyVersion, *m_source, *m_reviewer, *m_reason;
    QCheckBox *m_confirmed;
    QLabel *m_status, *m_gates;
    QPushButton *m_evaluate, *m_cancel, *m_save, *m_approve, *m_activate, *m_deactivate, *m_rollback;
    QPushButton *m_reloadPolicy;
};
#endif
