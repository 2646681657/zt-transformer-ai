#ifndef LEARNINGTRAININGPANEL_H
#define LEARNINGTRAININGPANEL_H
#include "SelfLearningDataset.h"
#include <QWidget>
#include <QProcess>

class QLineEdit;
class QPushButton;
class QSpinBox;
class QProgressBar;
class QLabel;
class QPlainTextEdit;
class QComboBox;
class QCheckBox;
class LearningReviewPanel;

class LearningTrainingPanel : public QWidget {
    Q_OBJECT
public:
    explicit LearningTrainingPanel(const QString &directory, QWidget *parent = nullptr);
    ~LearningTrainingPanel() override;
    void setRuntime(const QString &python, const QString &script);
    void setDataset(const QVector<SelfLearning::Sample> &samples);
    void setDatasetSource(const QString &path);
    void setPreviewSample(const QJsonObject &sample);
    bool isBusy() const;
    void requestCancel();
signals:
    void predictionInvalidated();
    void predictionReady(const QJsonObject &predictions, bool testOnly, const QString &modelVersion, bool approved = false);
private:
    enum Mode { Idle, Probe, Train, Predict };
    void probe();
    void startTraining();
    void predict();
    void predictDefault();
    void invalidateDefault();
    bool launch(Mode mode, const QStringList &args);
    void finish(int code, QProcess::ExitStatus status);
    void consumeOutput();
    void refresh();
    void history();
    void selectModel();
    void status(const QString &message);
    bool validRuntime() const;
    void saveRuntime();
    QString m_directory, m_job, m_model, m_datasetPath;
    QString m_launchedPython, m_launchedScript;
    QProcess m_process;
    Mode m_mode = Idle;
    bool m_probed = false, m_cancelled = false;
    bool m_initializing = true;
    QVector<QPushButton *> m_browse;
    quint64 m_generation = 0;
    quint64 m_displayGeneration = 0, m_predictionGeneration = 0;
    QByteArray m_stdout, m_stderr;
    QJsonObject m_prediction, m_preview, m_previewStarted;
    QVector<SelfLearning::Sample> m_samples;
    QJsonObject m_readiness;
    QLineEdit *m_python = nullptr, *m_script = nullptr;
    QPushButton *m_probe = nullptr, *m_train = nullptr, *m_cancel = nullptr, *m_predict = nullptr;
    QSpinBox *m_epochs = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_status = nullptr;
    QPlainTextEdit *m_report = nullptr;
    QComboBox *m_history = nullptr;
    QCheckBox *m_testOnly = nullptr;
    LearningReviewPanel *m_review = nullptr;
    QPushButton *m_defaultPredict = nullptr;
    bool m_approvedPrediction = false;
    QString m_predictionModel, m_observedDefault;
    QByteArray m_predictionModelHash, m_predictionReportHash, m_predictionRegistryHash, m_observedRegistryHash;
};
#endif
