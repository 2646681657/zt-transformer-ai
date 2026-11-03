#include "LearningTrainingPanel.h"
#include "LearningReviewPanel.h"
#include "LearningModelReview.h"
#include "LearningDataPreparation.h"
#include <QCryptographicHash>
#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QSet>
#include <cmath>

namespace {
QByteArray artifactHash(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly) || f.size() > 16 * 1024 * 1024) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
}
QJsonObject readObject(const QString &path)
{
    if (path.isEmpty()) return {};
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 16 * 1024 * 1024) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
bool experimental(const QJsonObject &o)
{
    return o.value("format") == QJsonValue("ZTBLD-ResidualModel") && o.value("version") == QJsonValue(1)
        && o.value("status") == QJsonValue("experimental") && o.value("productionEligible") == QJsonValue(false)
        && o.value("testOnly").isBool() && !o.value("modelVersion").toString().isEmpty();
}
bool finiteTensor(const QJsonValue &value, const QVector<int> &shape, int level = 0)
{
    if (level == shape.size()) return value.isDouble() && std::isfinite(value.toDouble());
    if (!value.isArray() || value.toArray().size() != shape[level]) return false;
    for (const auto &item : value.toArray()) if (!finiteTensor(item, shape, level + 1)) return false;
    return true;
}
bool completeArtifacts(const QJsonObject &model, const QJsonObject &report)
{
    if (!experimental(model) || report.value("status") != QJsonValue("completed")
        || report.value("modelVersion") != model.value("modelVersion")
        || report.value("testOnly") != model.value("testOnly") || report.value("productionEligible") != QJsonValue(false)
        || model.value("engineVersion") != QJsonValue(SelfLearning::engineVersion())) return false;
    const auto targets = model.value("models").toObject(), validation = report.value("validation").toObject();
    const int features = model.value("featureOrder").toArray().size();
    const QStringList expectedFeatures{"capacity_kVA", "hvRated_kV", "lvRated_kV", "coreDiameter_mm",
        "coreStraight_mm", "stackFactor", "steelThickness_mm", "hvBareWidth_mm", "hvBareThick_mm",
        "lvTurns", "lvFoilThick_mm", "lvFoilWidth_mm", "mainDuctWidth_mm", "refFluxDens_T", "hvCopperWire", "lvCopperFoil"};
    if (model.value("featureOrder").toArray() != QJsonArray::fromStringList(expectedFeatures)
        || model.value("architecture").toArray() != QJsonArray{16, 32, 16, 1}
        || model.value("scope") != QJsonValue("oil-foil-stacked-ellipse-tyoke-dual-cylinder-50hz-75c")
        || !model.value("fixedContext").isObject()
        || model.value("inputSchema").toObject().value("id") != QJsonValue("ztbld-calcinput-learning-v1")) return false;
    if (features <= 0 || features > 1000 || targets.isEmpty() || targets.keys() != validation.keys()) return false;
    QStringList allowed; for (const auto &metric : SelfLearning::metrics()) allowed.append(metric.key);
    for (auto it = targets.begin(); it != targets.end(); ++it) {
        if (!allowed.contains(it.key())) return false;
        const auto target = it.value().toObject(), weights = target.value("weights").toObject();
        const auto normalization = target.value("normalization").toObject(), bounds = target.value("bounds").toObject();
        if (!finiteTensor(normalization.value("mean"), {features}) || !finiteTensor(normalization.value("scale"), {features})
            || !finiteTensor(bounds.value("min"), {features}) || !finiteTensor(bounds.value("max"), {features})
            || !finiteTensor(normalization.value("targetMean"), {}) || !finiteTensor(normalization.value("targetScale"), {})
            || normalization.value("targetScale").toDouble() <= 0) return false;
        const auto scales = normalization.value("scale").toArray(), low = bounds.value("min").toArray(), high = bounds.value("max").toArray();
        for (int i = 0; i < features; ++i) if (scales[i].toDouble() <= 0 || low[i].toDouble() > high[i].toDouble()) return false;
        const auto scores = validation.value(it.key()).toObject();
        for (const auto key : {"baselineMAE", "modelMAE", "RMSE"})
            if (!scores.value(key).isDouble() || !std::isfinite(scores.value(key).toDouble()) || scores.value(key).toDouble() < 0) return false;
        if (scores != target.value("validation").toObject()
            || !finiteTensor(weights.value("0.weight"), {32, features}) || !finiteTensor(weights.value("0.bias"), {32})
            || !finiteTensor(weights.value("2.weight"), {16, 32}) || !finiteTensor(weights.value("2.bias"), {16})
            || !finiteTensor(weights.value("4.weight"), {1, 16}) || !finiteTensor(weights.value("4.bias"), {1})) return false;
    }
    return true;
}
}

LearningTrainingPanel::LearningTrainingPanel(const QString &directory, QWidget *parent)
    : QWidget(parent), m_directory(directory), m_datasetPath(directory+"/samples.json")
{
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    const auto runtimeRow = [&](const QString &label, const char *name, bool python) {
        auto *row = new QHBoxLayout;
        auto *edit = new QLineEdit(this); edit->setObjectName(name);
        auto *browse = new QPushButton(QStringLiteral("选择…"), this); browse->setAutoDefault(false);
        browse->setObjectName(python ? "browseTrainingPython" : "browseTrainingScript"); m_browse.append(browse);
        row->addWidget(edit, 1); row->addWidget(browse); form->addRow(label, row);
        connect(browse, &QPushButton::clicked, this, [this, edit, python] {
            const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("选择本地训练组件"), QString(),
                python ? "Python (python.exe)" : "Python (*.py)");
            if (!path.isEmpty()) { edit->setText(path); saveRuntime(); }
        });
        connect(edit, &QLineEdit::textChanged, this, [this] { m_probed = false; refresh(); });
        connect(edit, &QLineEdit::editingFinished, this, &LearningTrainingPanel::saveRuntime);
        return edit;
    };
    m_python = runtimeRow(QStringLiteral("独立环境python.exe"), "trainingPython", true);
    m_script = runtimeRow(QStringLiteral("本地trainer.py"), "trainingScript", false);
    m_epochs = new QSpinBox(this); m_epochs->setRange(1, 5000); m_epochs->setValue(200);
    form->addRow(QStringLiteral("最大训练轮数（CPU）"), m_epochs); layout->addLayout(form);
    m_testOnly = new QCheckBox(QStringLiteral("使用测试/合成数据（模型和报告永久标为测试，禁止上线）"), this);
    m_testOnly->setObjectName("testOnlyTraining"); layout->addWidget(m_testOnly);
    auto *buttons = new QHBoxLayout;
    const auto button = [&](const QString &name, const char *id) {
        auto *b = new QPushButton(name, this); b->setObjectName(id); b->setAutoDefault(false); buttons->addWidget(b); return b;
    };
    m_probe = button(QStringLiteral("检查训练环境"), "probeBackend");
    m_train = button(QStringLiteral("开始本地训练"), "startTraining");
    m_cancel = button(QStringLiteral("取消任务"), "cancelTraining");
    layout->addLayout(buttons);
    m_status = new QLabel(QStringLiteral("未配置独立训练环境；不会自动安装或上传数据。"), this);
    m_status->setObjectName("backendStatus"); m_status->setWordWrap(true); m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    m_progress = new QProgressBar(this); m_progress->setRange(0, 100); layout->addWidget(m_progress);
    m_history = new QComboBox(this); m_history->setObjectName("modelHistory"); layout->addWidget(m_history);
    auto *reports = new QHBoxLayout;
    auto *reload = new QPushButton(QStringLiteral("刷新本地模型版本"), this); reload->setAutoDefault(false); reload->setObjectName("reloadModelHistory");
    m_predict = new QPushButton(QStringLiteral("对当前样本独立预测（不参与寻优）"), this); m_predict->setObjectName("previewPrediction"); m_predict->setAutoDefault(false);
    reports->addWidget(reload); reports->addWidget(m_predict); layout->addLayout(reports);
    m_defaultPredict = new QPushButton(QStringLiteral("使用已审核默认模型独立预测"), this);
    m_defaultPredict->setObjectName("predictApprovedDefault"); m_defaultPredict->setAutoDefault(false); layout->addWidget(m_defaultPredict);
    m_report = new QPlainTextEdit(this); m_report->setReadOnly(true); m_report->setMaximumBlockCount(1500); m_report->setMinimumHeight(160);
    layout->addWidget(m_report);
    auto *hint = new QLabel(QStringLiteral("每目标至少5个有标签的独立产品才可运行验证，不代表已满足工程上线要求。按产品分组验证；测试模型永不发布。训练完成不等于优于原公式。"), this);
    hint->setWordWrap(true); layout->addWidget(hint);
    m_review = new LearningReviewPanel(m_directory, this); layout->addWidget(m_review);
    connect(m_review, &LearningReviewPanel::busyChanged, this, &LearningTrainingPanel::refresh);
    connect(m_review, &LearningReviewPanel::registryChanged, this, [this] { invalidateDefault(); refresh(); });
    connect(m_defaultPredict, &QPushButton::clicked, this, &LearningTrainingPanel::predictDefault);
    connect(m_testOnly, &QCheckBox::toggled, this, [this] { refresh(); });
    auto *registryWatch = new QTimer(this); registryWatch->setInterval(1500);
    connect(registryWatch, &QTimer::timeout, this, [this] {
        QString error; const auto active = LearningReview::activeModel(m_directory + "/review", error);
        const auto snapshot = artifactHash(m_directory + "/review/registry/registry.json");
        if (active != m_observedDefault || snapshot != m_observedRegistryHash) { m_observedDefault = active; m_observedRegistryHash = snapshot; invalidateDefault(); refresh(); }
    }); registryWatch->start();
    connect(m_probe, &QPushButton::clicked, this, &LearningTrainingPanel::probe);
    connect(m_train, &QPushButton::clicked, this, &LearningTrainingPanel::startTraining);
    connect(m_cancel, &QPushButton::clicked, this, &LearningTrainingPanel::requestCancel);
    connect(m_predict, &QPushButton::clicked, this, &LearningTrainingPanel::predict);
    connect(reload, &QPushButton::clicked, this, &LearningTrainingPanel::history);
    connect(m_history, &QComboBox::currentIndexChanged, this, &LearningTrainingPanel::selectModel);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &LearningTrainingPanel::consumeOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_stderr += m_process.readAllStandardError(); m_stderr = m_stderr.right(32768);
    });
    connect(&m_process, &QProcess::finished, this, &LearningTrainingPanel::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { m_stderr += m_process.errorString().toUtf8(); finish(-1, QProcess::CrashExit); }
    });
    auto config = readObject(m_directory + "/runtime.json");
    if (config.isEmpty()) config = readObject(QCoreApplication::applicationDirPath() + "/learning/runtime.json");
    setRuntime(config.value("python").toString(), config.value("script").toString());
    m_initializing = false; history(); refresh();
}

LearningTrainingPanel::~LearningTrainingPanel()
{
    if (m_process.state() != QProcess::NotRunning) { m_process.disconnect(this); m_process.kill(); m_process.waitForFinished(1500); }
}
bool LearningTrainingPanel::isBusy() const { return m_process.state() != QProcess::NotRunning || (m_review && m_review->isBusy()); }
void LearningTrainingPanel::status(const QString &text) { m_status->setText(text); }
void LearningTrainingPanel::setRuntime(const QString &python, const QString &script)
{
    if (isBusy()) return;
    m_python->setText(python); m_script->setText(script); m_probed = false; refresh();
    if (!m_initializing) saveRuntime();
}
void LearningTrainingPanel::saveRuntime()
{
    if (m_initializing || isBusy()) return;
    QString error;
    if (!SelfLearning::writeJson(m_directory + "/runtime.json", {{"python", m_python->text()}, {"script", m_script->text()}}, error)) status(error);
}
void LearningTrainingPanel::setDataset(const QVector<SelfLearning::Sample> &samples) {
    bool changed=m_samples.size()!=samples.size();
    if(!changed) for(int i=0;i<samples.size();++i) if(SelfLearning::encode(m_samples[i])!=SelfLearning::encode(samples[i])) {changed=true;break;}
    if(changed || m_readiness.isEmpty()) {m_samples=samples; m_readiness=LearningPreparation::inspect(m_samples);}
    if(m_review) m_review->setDataset(samples); refresh();
}
void LearningTrainingPanel::setDatasetSource(const QString &path) {
    if(isBusy()) return; m_datasetPath=path; if(m_review) m_review->setDatasetSource(path);
}
void LearningTrainingPanel::setPreviewSample(const QJsonObject &sample) { if (m_preview != sample) invalidateDefault(); m_preview = sample; refresh(); }
bool LearningTrainingPanel::validRuntime() const { return QFileInfo(m_python->text()).isFile() && QFileInfo(m_script->text()).isFile(); }
void LearningTrainingPanel::refresh()
{
    if (!m_probe) return; // Runtime edits can emit during construction.
    const bool busy = isBusy();
    for (auto *button : m_browse) button->setEnabled(!busy);
    m_python->setEnabled(!busy); m_script->setEnabled(!busy); m_epochs->setEnabled(!busy); m_testOnly->setEnabled(!busy);
    bool enough = false;
    for (int i = 0; i < SelfLearning::MetricCount; ++i) {
        QSet<QString> groups;
        for (const auto &s : m_samples) if (s.purpose=="training" && s.measured[i]) groups.insert(s.productId.trimmed().toCaseFolded());
        enough |= groups.size() >= 5;
    }
    const auto &preparation=m_readiness;
    bool valid=true; for(const auto &issue:preparation["issues"].toArray()) valid &= issue.toObject()["severity"]!=QJsonValue("error");
    m_train->setEnabled(!busy && m_probed && validRuntime() && enough && valid && preparation["trainingContextCount"].toInt()<=1);
    m_probe->setEnabled(!busy); m_cancel->setEnabled(busy);
    m_history->setEnabled(!busy);
    m_predict->setEnabled(!busy && m_probed && !m_preview.isEmpty() && QFileInfo::exists(m_model));
    if (m_review) {
        m_review->setRuntime(m_python->text(), m_script->text(), m_probed && !busy, m_testOnly->isChecked());
        QString error; const auto active = LearningReview::activeModel(m_directory + "/review", error);
        m_defaultPredict->setEnabled(!busy && m_probed && !m_preview.isEmpty() && !active.isEmpty());
    }
}
bool LearningTrainingPanel::launch(Mode mode, const QStringList &args)
{
    if (isBusy()) return false;
    if (!validRuntime()) { status(QStringLiteral("找不到python.exe或trainer.py，请选择有效的本地路径。")); refresh(); return false; }
    m_mode = mode; m_cancelled = false; ++m_generation; m_stdout.clear(); m_stderr.clear(); m_prediction = {};
    m_launchedPython = m_python->text(); m_launchedScript = m_script->text();
    auto env = QProcessEnvironment::systemEnvironment(); env.insert("PYTHONIOENCODING", "utf-8"); env.insert("PYTHONUTF8", "1");
    m_process.setProcessEnvironment(env);
    m_process.setProgram(m_python->text()); m_process.setArguments(QStringList{"-I", "-u", m_script->text()} + args);
    m_process.start(); refresh(); return true;
}
void LearningTrainingPanel::probe() { status(QStringLiteral("正在检查CPU训练后端…")); launch(Probe, {"probe"}); }
void LearningTrainingPanel::startTraining()
{
    if (!m_train->isEnabled()) return;
    if (!validRuntime()) {
        m_probed = false; status(QStringLiteral("训练组件在检查后被移动或删除，请重新选择并检查环境。")); refresh(); return;
    }
    QJsonObject bundle; QString error;
    if (!SelfLearning::freezeDatasetBundle(m_datasetPath,m_samples,{},false,bundle,error)) { status(QStringLiteral("冻结训练数据失败，请重新读取学习样本：%1").arg(error)); return; }
    m_job = m_directory + "/jobs/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!SelfLearning::writeJson(m_job + "/bundle.json", bundle, error)) { status(error); return; }
    m_progress->setValue(0); status(QStringLiteral("训练中；独立分组验证，结果仅供评估。"));
    if (!SelfLearning::writeJson(m_job + "/job.json", {{"status", "running"}, {"productionEligible", false}}, error)) { status(error); return; }
    QStringList args{"train", "--bundle", m_job + "/bundle.json", "--output", m_job,
        "--epochs", QString::number(m_epochs->value()), "--cancel-file", m_job + "/cancel"};
    if (m_testOnly->isChecked()) args.append("--test-only");
    if (!launch(Train, args)) {
        if (!SelfLearning::writeJson(m_job + "/job.json", {{"status", "failed"}, {"productionEligible", false}, {"diagnostics", "Runtime unavailable before launch"}}, error))
            status(QStringLiteral("无法保存任务失败状态：%1").arg(error));
    }
}
void LearningTrainingPanel::requestCancel()
{
    if (m_review && m_review->isBusy()) { m_review->requestCancel(); return; }
    if (!isBusy()) return;
    m_cancelled = true;
    if (m_mode == Train) { QSaveFile marker(m_job + "/cancel"); if (marker.open(QIODevice::WriteOnly)) { marker.write("cancel"); marker.commit(); } }
    else m_process.terminate();
    status(QStringLiteral("正在取消；本任务不会发布或启用模型。"));
    const auto generation = m_generation;
    QTimer::singleShot(5000, this, [this, generation] { if (isBusy() && generation == m_generation && m_cancelled) m_process.kill(); });
}
void LearningTrainingPanel::consumeOutput()
{
    m_stdout += m_process.readAllStandardOutput();
    if (m_stdout.size() > 1024 * 1024) { m_stderr += "Backend protocol line too long"; m_stdout.clear(); m_process.kill(); return; }
    qsizetype index;
    while ((index = m_stdout.indexOf('\n')) >= 0) {
        const auto line = m_stdout.left(index).trimmed(); m_stdout.remove(0, index + 1);
        auto object = QJsonDocument::fromJson(line).object();
        if (object.value("event") == QJsonValue("progress")) {
            m_progress->setValue(qBound(0, object.value("percent").toInt(), 99));
            status(object.value("message").toString());
        }
        if (object.value("event") == QJsonValue("error")) m_stderr += object.value("message").toString().toUtf8().left(8192);
        if (object.value("predictions").isObject()) m_prediction = object.value("predictions").toObject();
    }
}
void LearningTrainingPanel::finish(int code, QProcess::ExitStatus exitStatus)
{
    if (m_mode == Idle) return;
    consumeOutput();
    if (!m_stdout.trimmed().isEmpty()) { m_stdout.append('\n'); consumeOutput(); }
    const auto mode = m_mode; m_mode = Idle;
    const bool ok = !m_cancelled && code == 0 && exitStatus == QProcess::NormalExit;
    if (mode == Probe) { m_probed = ok && m_launchedPython == m_python->text() && m_launchedScript == m_script->text(); status(m_probed ? QStringLiteral("CPU训练环境检查通过。数据满足门槛后可开始训练。") : QStringLiteral("环境检查失败或配置已更改：%1").arg(QString::fromUtf8(m_stderr))); }
    else if (mode == Train) {
        const auto report = readObject(m_job + "/report.json"), model = readObject(m_job + "/model.json");
        const bool complete = ok && completeArtifacts(model, report);
        QString error;
        const bool saved = SelfLearning::writeJson(m_job + "/job.json", {{"status", m_cancelled ? "cancelled" : complete ? "completed" : "failed"}, {"productionEligible", false}, {"diagnostics", QString::fromUtf8(m_stderr).right(32768)}}, error);
        if (!saved) status(QStringLiteral("无法保存任务终态：%1").arg(error));
        else if (complete) { m_progress->setValue(100); status(QStringLiteral("训练及验证完成；模型仍为实验版本，不能参与寻优。")); history(); }
        else status(m_cancelled ? QStringLiteral("任务已取消，未启用模型。") : QStringLiteral("训练失败或产物不完整：%1").arg(QString::fromUtf8(m_stderr)));
    } else if (mode == Predict) {
        bool finite = !m_prediction.isEmpty();
        for (auto it = m_prediction.begin(); it != m_prediction.end(); ++it) finite &= it.value().isDouble() && std::isfinite(it.value().toDouble());
        QString error;
        const bool approved = (!m_approvedPrediction || LearningReview::activeModel(m_directory + "/review", error) == m_predictionModel)
            && artifactHash(m_directory + "/review/registry/registry.json") == m_predictionRegistryHash;
        const bool intact = !m_predictionModelHash.isEmpty() && artifactHash(m_predictionModel) == m_predictionModelHash
            && artifactHash(QFileInfo(m_predictionModel).absolutePath() + "/report.json") == m_predictionReportHash;
        if (ok && finite && approved && intact && m_preview == m_previewStarted && m_predictionGeneration == m_displayGeneration) {
            const auto model = readObject(m_predictionModel);
            emit predictionReady(m_prediction, model.value("testOnly").toBool(), model.value("modelVersion").toString(), m_approvedPrediction);
            status(QStringLiteral("独立预测完成，仅供验证；未修改原公式或寻优结果。"));
        } else { emit predictionInvalidated(); status(QStringLiteral("无法独立预测（样本已切换，或版本/范围/运行错误）：%1").arg(QString::fromUtf8(m_stderr))); }
    }
    refresh();
}
void LearningTrainingPanel::history()
{
    if (isBusy()) return;
    m_history->clear(); m_model.clear();
    const QDir jobs(m_directory + "/jobs");
    for (const auto &entry : jobs.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time)) {
        const auto job = readObject(entry.filePath() + "/job.json");
        const auto model = readObject(entry.filePath() + "/model.json");
        if (job.value("status") != QJsonValue("completed") || !completeArtifacts(model, readObject(entry.filePath() + "/report.json"))) continue;
        const auto badge = model.value("testOnly").toBool() ? QStringLiteral("[测试模型，禁止上线] ") : QStringLiteral("[实验模型，待工程验证] ");
        m_history->addItem(badge + model.value("modelVersion").toString(), entry.filePath());
    }
    selectModel();
}
void LearningTrainingPanel::selectModel()
{
    if (isBusy()) return;
    invalidateDefault();
    const auto dir = m_history->currentData().toString();
    m_model = dir.isEmpty() ? QString() : dir + "/model.json";
    if (m_review) {
        QStringList paths; for (int i = 0; i < m_history->count(); ++i) paths << m_history->itemData(i).toString() + "/model.json";
        m_review->setModels(paths, m_model);
    }
    const auto report = readObject(dir + "/report.json");
    QString summary;
    if (!dir.isEmpty()) {
        summary = QStringLiteral("模型：%1\n%2\n验证集用于选择最佳轮次，不是独立测试集，也不是上线精度证明。\n")
            .arg(report.value("modelVersion").toString(), report.value("testOnly").toBool()
                ? QStringLiteral("测试数据模型，禁止上线") : QStringLiteral("实验模型，待工程验证"));
        const auto validation = report.value("validation").toObject();
        for (const auto &metric : SelfLearning::metrics()) {
            const auto result = validation.value(metric.key).toObject();
            if (result.isEmpty()) { summary += QString::fromUtf8(metric.name) + QStringLiteral("：数据不足，未训练\n"); continue; }
            summary += QStringLiteral("%1：原公式MAE %2，DNN MAE %3，RMSE %4 %5（验证样本%6）\n")
                .arg(QString::fromUtf8(metric.name)).arg(result.value("baselineMAE").toDouble(), 0, 'g', 6)
                .arg(result.value("modelMAE").toDouble(), 0, 'g', 6).arg(result.value("RMSE").toDouble(), 0, 'g', 6)
                .arg(metric.unit).arg(result.value("validationSamples").toInt());
        }
        summary += QStringLiteral("\n完整验证记录：\n");
    }
    m_report->setPlainText(dir.isEmpty() ? QStringLiteral("尚无完成的训练任务。这里将显示验证误差、原公式基线、组划分及模型版本。")
        : summary + QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
    refresh();
}
void LearningTrainingPanel::predict()
{
    if (!m_predict->isEnabled()) return;
    emit predictionInvalidated();
    QString error;
    const auto path = m_directory + "/preview-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
    if (!SelfLearning::writeJson(path, m_preview, error)) { status(error); return; }
    m_previewStarted = m_preview;
    m_predictionGeneration = m_displayGeneration;
    m_approvedPrediction = false; m_predictionModel = m_model;
    QString registryError; m_observedDefault = LearningReview::activeModel(m_directory + "/review", registryError);
    m_predictionRegistryHash = m_observedRegistryHash = artifactHash(m_directory + "/review/registry/registry.json");
    m_predictionModelHash = artifactHash(m_predictionModel);
    m_predictionReportHash = artifactHash(QFileInfo(m_predictionModel).absolutePath() + "/report.json");
    launch(Predict, {"predict", "--model", m_predictionModel, "--sample", path});
}
void LearningTrainingPanel::invalidateDefault() { ++m_displayGeneration; emit predictionInvalidated(); }
void LearningTrainingPanel::predictDefault()
{
    if (!m_defaultPredict->isEnabled()) return;
    emit predictionInvalidated(); QString error;
    const auto model = LearningReview::activeModel(m_directory + "/review", error);
    if (model.isEmpty()) { status(error); refresh(); return; }
    const auto path = m_directory + "/preview-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
    if (!SelfLearning::writeJson(path, m_preview, error)) { status(error); return; }
    m_approvedPrediction = true; m_predictionModel = model; m_previewStarted = m_preview;
    m_predictionGeneration = m_displayGeneration;
    m_predictionModelHash = artifactHash(model); m_predictionReportHash = artifactHash(QFileInfo(model).absolutePath() + "/report.json");
    m_predictionRegistryHash = artifactHash(m_directory + "/review/registry/registry.json");
    m_observedDefault = model; m_observedRegistryHash = m_predictionRegistryHash;
    launch(Predict, {"predict", "--model", model, "--sample", path});
}
