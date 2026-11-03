#pragma once
#include "IOptimizer.h"
#include <QDialog>
#include <QProcess>
#include <QJsonObject>
class QComboBox;
class QCheckBox;
class QTableWidget;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class CandidateComparisonDialog : public QDialog {
    Q_OBJECT
public:
    CandidateComparisonDialog(const QVector<OptimizeCandidate>&candidates,const QString&directory,QWidget *parent=nullptr);
    ~CandidateComparisonDialog() override;
    void setRuntime(const QString &python,const QString &script);
    bool isBusy() const;
public slots:
    void reject() override;
private:
    void reloadModels();
    void selectModel();
    void refresh();
    void start();
    void cancel();
    void consume();
    void finish(int exitCode,QProcess::ExitStatus status);
    void render();
    void invalidate(const QString &reason);
    QString m_directory,m_python,m_script,m_snapshotError,m_job,m_outputDirectory;
    QString m_startedModel,m_modelHash,m_trainingReportHash,m_registryHash,m_version;
    QJsonObject m_input,m_report;
    QProcess m_process;
    QByteArray m_stdout,m_stderr;
    bool m_running=false,m_cancelled=false,m_closeAfterCancel=false,m_invalidated=false,m_startedApproved=false;
    quint64 m_generation=0;
    QComboBox *m_models=nullptr;
    QCheckBox *m_experimental=nullptr;
    QTableWidget *m_values=nullptr;
    QLabel *m_status=nullptr,*m_badge=nullptr;
    QLineEdit *m_record=nullptr;
    QProgressBar *m_progress=nullptr;
    QPushButton *m_start=nullptr,*m_cancel=nullptr,*m_reload=nullptr;
};
