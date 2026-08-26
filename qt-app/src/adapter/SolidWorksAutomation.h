#ifndef SOLIDWORKSAUTOMATION_H
#define SOLIDWORKSAUTOMATION_H
// SOLIDWORKS 自动化适配器：通过 PowerShell + COM 执行参数修改和工程图导出

#include <QObject>
#include <QList>
#include <QString>

#include <memory>

class QProcess;
class QTemporaryDir;

struct SwDimensionParameter {
    QString name;          // SOLIDWORKS 完整尺寸名，例如 D1@Sketch1
    double valueMm = 0.0;  // 用户界面统一使用毫米
    QString description;
};

struct SwDrawingRequest {
    QString modelTemplatePath;
    QString drawingTemplatePath;
    QString outputDirectory;
    QString outputBaseName;
    QList<SwDimensionParameter> parameters;
    bool exportPdf = true;
    bool solidWorksVisible = true;
    bool insertModelDimensions = true;
};

class SolidWorksAutomation : public QObject {
    Q_OBJECT
public:
    explicit SolidWorksAutomation(QObject *parent = nullptr);
    ~SolidWorksAutomation() override;

    bool isRunning() const;
    static bool isSolidWorksRegistered();
    void generate(const SwDrawingRequest &request);

signals:
    void logMessage(const QString &message);
    void finished(bool success, const QString &message, const QStringList &outputFiles);

private:
    void finishWithError(const QString &message);
    void handleProcessFinished(int exitCode);

    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
};

#endif // SOLIDWORKSAUTOMATION_H
