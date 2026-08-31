#include "SolidWorksAutomation.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>

SolidWorksAutomation::SolidWorksAutomation(QObject *parent)
    : QObject(parent), m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QByteArray data = m_process->readAllStandardOutput();
        m_stdoutBuffer += data;
        const QString text = QString::fromUtf8(data).trimmed();
        if (!text.isEmpty()) {
            emit logMessage(text);
        }
    });

    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray data = m_process->readAllStandardError();
        m_stderrBuffer += data;
        const QString text = QString::fromLocal8Bit(data).trimmed();
        if (!text.isEmpty()) {
            emit logMessage(text);
        }
    });

    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finishWithError(QStringLiteral(
                "无法启动 Windows PowerShell。请确认系统中存在 powershell.exe。"));
        }
    });

    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        handleProcessFinished(exitCode);
    });
}

SolidWorksAutomation::~SolidWorksAutomation() = default;

bool SolidWorksAutomation::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

bool SolidWorksAutomation::isSolidWorksRegistered()
{
    const QStringList keys = {
        QStringLiteral("HKEY_CLASSES_ROOT\\SldWorks.Application\\CLSID"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\SldWorks.Application\\CLSID")
    };

    for (const QString &key : keys) {
        QSettings settings(key, QSettings::NativeFormat);
        if (!settings.value(QStringLiteral(".")).toString().isEmpty()
            || !settings.value(QStringLiteral("Default")).toString().isEmpty()
            || !settings.allKeys().isEmpty()) {
            return true;
        }
    }
    return false;
}

void SolidWorksAutomation::generate(const SwDrawingRequest &request)
{
    if (isRunning()) {
        emit finished(false, QStringLiteral("已有一个 SOLIDWORKS 出图任务正在执行。"), {});
        return;
    }

    m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        finishWithError(QStringLiteral("无法创建临时工作目录。"));
        return;
    }

    const QString scriptPath = m_tempDir->filePath(QStringLiteral("solidworks_parametric.ps1"));
    QFile resource(QStringLiteral(":/scripts/solidworks_parametric.ps1"));
    if (!resource.open(QIODevice::ReadOnly)) {
        finishWithError(QStringLiteral("无法读取内置 SOLIDWORKS 自动化脚本。"));
        return;
    }
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly)) {
        finishWithError(QStringLiteral("无法创建临时 SOLIDWORKS 自动化脚本。"));
        return;
    }
    script.write(resource.readAll());
    script.close();

    QJsonObject root;
    root.insert(QStringLiteral("modelTemplatePath"), request.modelTemplatePath);
    root.insert(QStringLiteral("drawingTemplatePath"), request.drawingTemplatePath);
    root.insert(QStringLiteral("outputDirectory"), request.outputDirectory);
    root.insert(QStringLiteral("outputBaseName"), request.outputBaseName);
    root.insert(QStringLiteral("exportPdf"), request.exportPdf);
    root.insert(QStringLiteral("solidWorksVisible"), request.solidWorksVisible);
    root.insert(QStringLiteral("insertModelDimensions"), request.insertModelDimensions);

    QJsonArray parameters;
    for (const SwDimensionParameter &parameter : request.parameters) {
        QJsonObject item;
        item.insert(QStringLiteral("name"), parameter.name);
        item.insert(QStringLiteral("valueMm"), parameter.valueMm);
        item.insert(QStringLiteral("description"), parameter.description);
        parameters.append(item);
    }
    root.insert(QStringLiteral("parameters"), parameters);

    const QString requestPath = m_tempDir->filePath(QStringLiteral("request.json"));
    QFile requestFile(requestPath);
    if (!requestFile.open(QIODevice::WriteOnly)) {
        finishWithError(QStringLiteral("无法创建临时出图参数文件。"));
        return;
    }
    requestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    requestFile.close();

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    emit logMessage(QStringLiteral("正在启动 SOLIDWORKS 参数化出图任务……"));

    const QStringList arguments = {
        QStringLiteral("-NoLogo"),
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-File"),
        scriptPath,
        QStringLiteral("-RequestPath"),
        requestPath
    };
    m_process->start(QStringLiteral("powershell.exe"), arguments);
}

void SolidWorksAutomation::finishWithError(const QString &message)
{
    m_tempDir.reset();
    emit finished(false, message, {});
}

void SolidWorksAutomation::handleProcessFinished(int exitCode)
{
    const QString output = QString::fromUtf8(m_stdoutBuffer);
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    QJsonObject result;
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString line = it->trimmed();
        const QString marker = QStringLiteral("ZTF_RESULT:");
        if (!line.startsWith(marker)) {
            continue;
        }
        const QJsonDocument document =
            QJsonDocument::fromJson(line.mid(marker.size()).toUtf8());
        if (document.isObject()) {
            result = document.object();
        }
        break;
    }

    if (result.isEmpty()) {
        QString message = QString::fromLocal8Bit(m_stderrBuffer).trimmed();
        if (message.isEmpty()) {
            message = exitCode == 0
                ? QStringLiteral("SOLIDWORKS 任务结束，但没有返回有效结果。")
                : QStringLiteral("SOLIDWORKS 自动化任务执行失败，退出码：%1").arg(exitCode);
        }
        m_tempDir.reset();
        emit finished(false, message, {});
        return;
    }

    QStringList files;
    const QJsonArray outputFiles = result.value(QStringLiteral("outputFiles")).toArray();
    for (const QJsonValue &value : outputFiles) {
        files.append(value.toString());
    }

    const bool success = result.value(QStringLiteral("success")).toBool(false);
    const QString message = result.value(QStringLiteral("message")).toString(
        success ? QStringLiteral("参数化出图完成。")
                : QStringLiteral("参数化出图失败。"));

    m_tempDir.reset();
    emit finished(success, message, files);
}
