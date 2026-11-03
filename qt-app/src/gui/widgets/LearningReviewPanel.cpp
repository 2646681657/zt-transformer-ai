#include "LearningReviewPanel.h"
#include "LearningModelReview.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace {
QJsonObject objectAt(const QString &path) {
    if (path.isEmpty()) return {};
    QFile f(path); if (!f.open(QIODevice::ReadOnly) || f.size() > 16 * 1024 * 1024) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
QLabel *explanation(const QString &text, QWidget *parent) {
    auto *l = new QLabel(text, parent); l->setWordWrap(true); l->setTextFormat(Qt::PlainText); return l;
}
QString score(const QJsonObject &o, const char *key) {
    return o[key].isDouble() ? QString::number(o[key].toDouble(), 'g', 7) : QStringLiteral("—");
}
QString policyDigest(const QJsonObject &policy) {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(policy).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
QString readableGate(QString text) {
    const QList<QPair<QString, QString>> messages{
        {"No supported enterprise policy", QStringLiteral("尚未保存已确认的企业审核标准。")},
        {"Missing artifact path", QStringLiteral("请选择模型版本及对应的独立比较记录。")},
        {"Test-only evidence forbidden", QStringLiteral("测试模型或测试证据不能用于审核和启用。")},
        {"Synthetic evidence forbidden", QStringLiteral("合成数据证据不能用于审核和启用。")},
        {"No current approval", QStringLiteral("所选版本尚无有效人工审核。")},
        {"Approval revoked", QStringLiteral("旧审核已失效，需要按当前企业标准重新审核。")},
        {"No valid historically active approval for rollback", QStringLiteral("没有仍有效的历史已启用版本可供回退。")}};
    for (const auto &entry : messages) if (text == entry.first) return entry.second;
    return text;
}
}

LearningReviewPanel::LearningReviewPanel(const QString &directory, QWidget *parent)
    : QWidget(parent), m_directory(directory), m_datasetPath(directory+"/samples.json")
{
    auto *root = new QVBoxLayout(this);
    auto *tests = new QGroupBox(QStringLiteral("独立测试与模型比较"), this);
    auto *tl = new QVBoxLayout(tests); root->addWidget(tests);
    tl->addWidget(explanation(QStringLiteral("勾选未参加该模型训练或验证的产品；所有模型使用同一冻结测试包。缺标签、越界、缺目标会明确记录，不以训练验证误差代替独立测试。"), this));
    auto *lists = new QHBoxLayout;
    m_sampleList = new QListWidget(this); m_sampleList->setObjectName("independentTestSamples"); m_sampleList->setMaximumHeight(165);
    m_sampleList->setToolTip(QStringLiteral("测试产品默认不选，请确认其未用于训练/验证；后台仍按产品组检查泄漏。"));
    m_models = new QListWidget(this); m_models->setObjectName("comparisonModels"); m_models->setMaximumHeight(165);
    lists->addWidget(m_sampleList, 1); lists->addWidget(m_models, 1); tl->addLayout(lists);
    auto *actions = new QHBoxLayout;
    m_evaluate = new QPushButton(QStringLiteral("冻结所选产品并比较模型"), this); m_evaluate->setObjectName("evaluateModels");
    m_cancel = new QPushButton(QStringLiteral("取消独立测试"), this);
    actions->addWidget(m_evaluate); actions->addWidget(m_cancel); tl->addLayout(actions);
    m_runs = new QComboBox(this); m_runs->setObjectName("independentTestHistory"); tl->addWidget(m_runs);
    m_results = new QTableWidget(0, 8, this); m_results->setObjectName("independentTestResults");
    m_results->setHorizontalHeaderLabels({QStringLiteral("模型/指标"), QStringLiteral("标签/产品"), QStringLiteral("覆盖率"), QStringLiteral("原MAE"), QStringLiteral("DNN MAE"), QStringLiteral("原/DNN RMSE"), QStringLiteral("原/DNN最大误差"), QStringLiteral("失败原因")});
    m_results->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_results->horizontalHeader()->setStretchLastSection(true); m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setMinimumHeight(160); tl->addWidget(m_results);
    tl->addWidget(explanation(QStringLiteral("MAE为平均绝对误差，RMSE为均方根误差；越小越好，单位与指标一致。原公式MAE为0时不计算百分比改善率。测试集用于比较后已参与模型选择，后续最终验收需补充未使用产品；本地记录不证明企业外部从未使用这些数据。"), this));
    auto *review = new QGroupBox(QStringLiteral("企业审核标准与默认独立展示模型"), this);
    auto *rl = new QVBoxLayout(review); root->addWidget(review);
    auto *form = new QFormLayout;
    m_policyVersion = new QLineEdit(this); m_policyVersion->setObjectName("approvalPolicyVersion");
    m_source = new QLineEdit(this); m_source->setObjectName("approvalPolicySource");
    m_confirmed = new QCheckBox(QStringLiteral("企业已确认以下标准（不预设合格值）"), this); m_confirmed->setObjectName("approvalPolicyConfirmed");
    form->addRow(QStringLiteral("策略版本 *"), m_policyVersion); form->addRow(QStringLiteral("门槛来源 *"), m_source); form->addRow(m_confirmed); rl->addLayout(form);
    m_policy = new QTableWidget(SelfLearning::MetricCount, 7, this); m_policy->setObjectName("approvalPolicyTargets");
    m_policy->setHorizontalHeaderLabels({QStringLiteral("审核目标"), QStringLiteral("单位"), QStringLiteral("最大MAE"), QStringLiteral("最大RMSE"), QStringLiteral("最大绝对误差"), QStringLiteral("最少产品数"), QStringLiteral("不劣原公式")});
    m_policy->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_policy->setMinimumHeight(220);
    for (int row = 0; row < SelfLearning::MetricCount; ++row) {
        const auto &metric = SelfLearning::metrics()[row];
        auto *name = new QTableWidgetItem(QString::fromUtf8(metric.name)); name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable); name->setCheckState(Qt::Unchecked); m_policy->setItem(row, 0, name);
        auto *unit = new QTableWidgetItem(metric.unit); unit->setFlags(Qt::ItemIsEnabled); m_policy->setItem(row, 1, unit);
        for (int col = 2; col < 6; ++col) m_policy->setItem(row, col, new QTableWidgetItem);
        auto *worse = new QTableWidgetItem; worse->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable); worse->setCheckState(Qt::Unchecked); m_policy->setItem(row, 6, worse);
    }
    rl->addWidget(m_policy);
    m_save = new QPushButton(QStringLiteral("保存企业确认标准（更改后旧审核失效）"), this); m_save->setObjectName("saveApprovalPolicy"); rl->addWidget(m_save);
    m_reloadPolicy = new QPushButton(QStringLiteral("重新载入已保存企业标准（放弃本页未保存修改）"), this); m_reloadPolicy->setObjectName("reloadApprovalPolicy"); rl->addWidget(m_reloadPolicy);
    auto *reviewer = new QFormLayout;
    m_reviewer = new QLineEdit(this); m_reviewer->setObjectName("modelReviewer");
    m_reason = new QLineEdit(this); m_reason->setObjectName("modelReviewReason");
    reviewer->addRow(QStringLiteral("审核人 *（本地记录，非身份认证）"), m_reviewer); reviewer->addRow(QStringLiteral("审核/停用理由 *"), m_reason); rl->addLayout(reviewer);
    auto *buttons = new QHBoxLayout;
    const auto button = [&](const QString &text, const char *name) { auto *b = new QPushButton(text, this); b->setObjectName(name); b->setAutoDefault(false); buttons->addWidget(b); return b; };
    m_approve = button(QStringLiteral("审核所选版本"), "approveDisplayModel");
    m_activate = button(QStringLiteral("设为默认独立模型"), "activateModel");
    m_deactivate = button(QStringLiteral("停用默认模型"), "deactivateModel");
    m_rollback = button(QStringLiteral("回退有效历史版本"), "rollbackModel"); rl->addLayout(buttons);
    m_gates = explanation(QString(), this); m_gates->setObjectName("modelApprovalBlockers"); rl->addWidget(m_gates);
    m_status = explanation(QString(), this); m_status->setObjectName("modelReviewStatus"); root->addWidget(m_status);
    root->addWidget(explanation(QStringLiteral("仅影响独立修正展示列；不更改原公式、合格判定、寻优、报价或打印。测试/合成模型和证据永不启用。审核与切换保存在本地审计记录。"), this));
    for (auto *b : findChildren<QPushButton *>()) b->setAutoDefault(false);
    connect(m_sampleList, &QListWidget::itemChanged, this, [this] { refresh(); });
    connect(m_models, &QListWidget::itemChanged, this, [this] { refresh(); });
    connect(m_evaluate, &QPushButton::clicked, this, &LearningReviewPanel::evaluate);
    connect(m_cancel, &QPushButton::clicked, this, &LearningReviewPanel::requestCancel);
    connect(m_runs, &QComboBox::currentIndexChanged, this, &LearningReviewPanel::displayRun);
    connect(m_save, &QPushButton::clicked, this, &LearningReviewPanel::savePolicy);
    connect(m_reloadPolicy, &QPushButton::clicked, this, [this] { reloadPolicy(); refresh(); });
    connect(m_policy, &QTableWidget::itemChanged, this, [this] { refresh(); });
    for (auto *edit : {m_policyVersion, m_source}) connect(edit, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(m_confirmed, &QCheckBox::toggled, this, [this] { refresh(); });
    connect(m_approve, &QPushButton::clicked, this, [this] {
        QString error;
        if (!policyVisibleAndCurrent(error)) { m_status->setText(error); refresh(); return; }
        if (LearningReview::approve(reviewDirectory(), m_selected, m_runs->currentData().toString(), m_reviewer->text(), m_reason->text(), error, policyDigest(m_displayedPolicy))) { m_status->setText(QStringLiteral("审核记录已保存；尚未设为默认。")); emit registryChanged(); }
        else m_status->setText(error);
        refresh();
    });
    connect(m_activate, &QPushButton::clicked, this, [this] {
        QString error; if (LearningReview::activate(reviewDirectory(), m_selected, error)) { m_status->setText(QStringLiteral("已设为默认独立展示模型；原公式保持不变。")); emit registryChanged(); } else m_status->setText(error); refresh();
    });
    connect(m_deactivate, &QPushButton::clicked, this, [this] {
        QString error; if (LearningReview::deactivate(reviewDirectory(), m_reason->text(), error)) { m_status->setText(QStringLiteral("已停用；未删除历史或样本。")); emit registryChanged(); } else m_status->setText(error); refresh();
    });
    connect(m_rollback, &QPushButton::clicked, this, [this] {
        QString error; if (LearningReview::rollback(reviewDirectory(), error)) { m_status->setText(QStringLiteral("已回退到仍有效的历史审核版本。")); emit registryChanged(); } else m_status->setText(error); refresh();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &LearningReviewPanel::consume);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_errors = (m_errors + m_process.readAllStandardError()).right(32768); });
    connect(&m_process, &QProcess::finished, this, &LearningReviewPanel::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) { if (e == QProcess::FailedToStart) { m_errors += m_process.errorString().toUtf8(); finish(-1, QProcess::CrashExit); } });
    reloadPolicy(); loadRuns(); refresh();
}

void LearningReviewPanel::reloadPolicy() {
    m_loading = true;
    QString error; const auto saved = LearningReview::policy(reviewDirectory(), error);
    m_displayedPolicy = saved;
    m_policyVersion->setText(saved["policyVersion"].toString()); m_source->setText(saved["source"].toString()); m_confirmed->setChecked(saved["confirmed"].toBool());
    for (int row = 0; row < SelfLearning::MetricCount; ++row) {
        m_policy->item(row, 0)->setCheckState(Qt::Unchecked);
        for (int col = 2; col < 6; ++col) m_policy->item(row, col)->setText(QString());
        m_policy->item(row, 6)->setCheckState(Qt::Unchecked);
        const auto target = saved["targets"].toObject()[SelfLearning::metrics()[row].key].toObject(); if (target.isEmpty()) continue;
        m_policy->item(row, 0)->setCheckState(Qt::Checked);
        const QStringList keys{"maxMAE", "maxRMSE", "maxAbsoluteError", "minProducts"};
        for (int col = 2; col < 6; ++col) m_policy->item(row, col)->setText(QString::number(target[keys[col - 2]].toDouble(), 'g', std::numeric_limits<double>::max_digits10));
        m_policy->item(row, 6)->setCheckState(target["notWorseThanBaseline"].toBool() ? Qt::Checked : Qt::Unchecked);
    }
    m_loading = false;
    if (!error.isEmpty()) m_status->setText(error);
}

LearningReviewPanel::~LearningReviewPanel() { if (isBusy()) { m_process.disconnect(this); m_process.kill(); m_process.waitForFinished(1500); } }
void LearningReviewPanel::setDataset(const QVector<SelfLearning::Sample> &samples) {
    if (m_samples.size() == samples.size()) { bool equal = true; for (int i = 0; i < samples.size(); ++i) equal &= SelfLearning::encode(m_samples[i]) == SelfLearning::encode(samples[i]); if (equal) return; }
    m_loading = true; m_samples = samples; m_sampleList->clear();
    for (const auto &s : samples) {
        auto *item = new QListWidgetItem(s.productId + " / " + s.testedOn.toString(Qt::ISODate), m_sampleList);
        item->setFlags(s.purpose=="independent_test"?Qt::ItemIsEnabled | Qt::ItemIsUserCheckable:Qt::ItemIsSelectable);
        item->setCheckState(Qt::Unchecked); item->setToolTip(s.source+QStringLiteral("；用途：")+SelfLearning::purposeName(s.purpose));
    }
    m_loading = false; refresh();
}
void LearningReviewPanel::setModels(const QStringList &models, const QString &selected) {
    m_loading = true; m_selected = selected; QStringList existing;
    for (int i = 0; i < m_models->count(); ++i) existing << m_models->item(i)->data(Qt::UserRole).toString();
    if (existing != models) { m_models->clear(); for (const auto &path : models) {
        const auto model = objectAt(path); auto *item = new QListWidgetItem((model["testOnly"].toBool() ? QStringLiteral("[测试] ") : QStringLiteral("[实验] ")) + model["modelVersion"].toString(), m_models);
        item->setData(Qt::UserRole, path); item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable); item->setCheckState(Qt::Checked);
    } }
    m_loading = false; refresh();
}
void LearningReviewPanel::setRuntime(const QString &python, const QString &script, bool ready, bool testOnly) { m_python = python; m_script = script; m_ready = ready; m_testOnly = testOnly; refresh(); }
void LearningReviewPanel::refresh() {
    if (m_loading) return;
    const bool busy = isBusy(); int samples = 0, models = 0;
    for (int i = 0; i < m_sampleList->count(); ++i) samples += m_sampleList->item(i)->checkState() == Qt::Checked && m_samples[i].purpose=="independent_test";
    for (int i = 0; i < m_models->count(); ++i) models += m_models->item(i)->checkState() == Qt::Checked;
    m_sampleList->setEnabled(!busy); m_models->setEnabled(!busy); m_runs->setEnabled(!busy);
    m_evaluate->setEnabled(!busy && m_ready && samples && models); m_cancel->setEnabled(busy);
    for (auto *w : QList<QWidget *>{m_policy, m_policyVersion, m_source, m_confirmed, m_save, m_reloadPolicy, m_reviewer, m_reason}) w->setEnabled(!busy);
    auto blockers = LearningReview::blockers(reviewDirectory(), m_selected, m_runs->currentData().toString());
    for (auto &message : blockers) message = readableGate(message);
    QString policyReason; if (!policyVisibleAndCurrent(policyReason)) blockers.prepend(policyReason);
    QString error; const auto active = LearningReview::activeModel(reviewDirectory(), error);
    m_gates->setText(QStringLiteral("默认独立展示模型：%1\n所选版本：%2\n%3").arg(active.isEmpty() ? QStringLiteral("未启用（或审核已失效）") : objectAt(active)["modelVersion"].toString(), objectAt(m_selected)["modelVersion"].toString(), blockers.isEmpty() ? QStringLiteral("独立证据满足策略；仍须人工审核并设为默认。") : blockers.join("\n")));
    m_approve->setEnabled(!busy && blockers.isEmpty());
    // Activation and rollback validate their complete audit chain again on click.
    const auto state = LearningReview::state(reviewDirectory(), error);
    m_activate->setEnabled(!busy && policyReason.isEmpty() && LearningReview::canActivate(reviewDirectory(), m_selected, error));
    m_activate->setToolTip(error);
    m_deactivate->setEnabled(!busy && !state.value("activeModel").toString().isEmpty());
    m_rollback->setEnabled(!busy && LearningReview::canRollback(reviewDirectory(), error));
    m_rollback->setToolTip(error);
}
void LearningReviewPanel::savePolicy() {
    QString currentError; const auto current = LearningReview::policy(reviewDirectory(), currentError);
    if (!currentError.isEmpty() || current != m_displayedPolicy) { m_status->setText(QStringLiteral("审核标准已被其他窗口更改或无法读取，请先重新载入后再保存。")); refresh(); return; }
    QJsonObject targets;
    for (int row = 0; row < SelfLearning::MetricCount; ++row) if (m_policy->item(row, 0)->checkState() == Qt::Checked) {
        QJsonObject target; const QStringList keys{"maxMAE", "maxRMSE", "maxAbsoluteError", "minProducts"};
        for (int col = 2; col < 6; ++col) { bool ok = false; const auto value = m_policy->item(row, col)->text().toDouble(&ok); if (!ok || !std::isfinite(value) || value < 0 || (col == 5 && (value < 1 || value != std::floor(value)))) { m_status->setText(QStringLiteral("请填写所选目标的全部有限非负误差门槛及正整数产品数；不提供默认值。")); return; } target[keys[col - 2]] = value; }
        target["notWorseThanBaseline"] = m_policy->item(row, 6)->checkState() == Qt::Checked;
        targets[SelfLearning::metrics()[row].key] = target;
    }
    QJsonObject policy{{"format", "ZTBLD-DisplayApprovalPolicy"}, {"version", 1}, {"policyVersion", m_policyVersion->text().trimmed()}, {"source", m_source->text().trimmed()}, {"confirmed", m_confirmed->isChecked()}, {"targets", targets}};
    QString error; if (LearningReview::savePolicy(reviewDirectory(), policy, error, policyDigest(m_displayedPolicy))) { m_displayedPolicy = policy; m_status->setText(QStringLiteral("企业标准已保存；所有模型须按当前标准重新核验，旧展示已清除。")); emit registryChanged(); } else m_status->setText(error); refresh();
}
QJsonObject LearningReviewPanel::editorPolicy() const {
    QJsonObject targets;
    const QStringList keys{"maxMAE", "maxRMSE", "maxAbsoluteError", "minProducts"};
    for (int row = 0; row < SelfLearning::MetricCount; ++row) if (m_policy->item(row, 0)->checkState() == Qt::Checked) {
        QJsonObject target;
        for (int col = 2; col < 6; ++col) { bool ok = false; const double value = m_policy->item(row, col)->text().toDouble(&ok); if (!ok || !std::isfinite(value)) return {}; target[keys[col - 2]] = value; }
        target["notWorseThanBaseline"] = m_policy->item(row, 6)->checkState() == Qt::Checked;
        targets[SelfLearning::metrics()[row].key] = target;
    }
    return {{"format", "ZTBLD-DisplayApprovalPolicy"}, {"version", 1}, {"policyVersion", m_policyVersion->text().trimmed()}, {"source", m_source->text().trimmed()}, {"confirmed", m_confirmed->isChecked()}, {"targets", targets}};
}
bool LearningReviewPanel::policyVisibleAndCurrent(QString &reason) const {
    QString error; const auto saved = LearningReview::policy(reviewDirectory(), error);
    if (!error.isEmpty()) { reason = error; return false; }
    if (saved != m_displayedPolicy) { reason = QStringLiteral("审核标准已被其他窗口更改，请先重新载入；当前编辑内容不会被自动覆盖。建议重新查看独立比较结果。 "); return false; }
    if (saved.isEmpty() || editorPolicy() != saved) { reason = QStringLiteral("审核标准尚未保存，或当前编辑内容与已保存标准不一致；请先保存企业确认标准，或重新载入。 "); return false; }
    return true;
}
void LearningReviewPanel::evaluate() {
    if (!m_evaluate->isEnabled()) return;
    QStringList selected; for (int i = 0; i < m_sampleList->count(); ++i) if (m_sampleList->item(i)->checkState() == Qt::Checked && m_samples[i].purpose=="independent_test") selected.append(m_samples[i].id);
    QJsonObject bundle; QString error; if (!SelfLearning::freezeDatasetBundle(m_datasetPath,m_samples,selected,true,bundle,error)) { m_status->setText(QStringLiteral("冻结独立测试失败，请重新读取学习样本：%1").arg(error)); return; }
    m_run = reviewDirectory() + "/test_runs/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_evaluationDirectory = m_run + "/evaluation-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!SelfLearning::writeJson(m_run + "/bundle.json", bundle, error)) { m_status->setText(error); return; }
    QStringList args{"-I", "-u", m_script, "evaluate", "--bundle", m_run + "/bundle.json", "--output", m_evaluationDirectory, "--cancel-file", m_run + "/cancel", "--models"};
    for (int i = 0; i < m_models->count(); ++i) if (m_models->item(i)->checkState() == Qt::Checked) args << m_models->item(i)->data(Qt::UserRole).toString();
    if (m_testOnly) args << "--test-only";
    if (!SelfLearning::writeJson(m_run + "/job.json", {{"status", "running"}}, error)) { m_status->setText(error); return; }
    m_cancelled = false; ++m_generation; m_output.clear(); m_errors.clear();
    auto env = QProcessEnvironment::systemEnvironment(); env.insert("PYTHONIOENCODING", "utf-8"); env.insert("PYTHONUTF8", "1"); m_process.setProcessEnvironment(env);
    m_process.setProgram(m_python); m_process.setArguments(args); m_process.start(); m_status->setText(QStringLiteral("正在冻结同批产品并逐模型独立测试；不会修改模型权重。")); refresh(); emit busyChanged();
}
void LearningReviewPanel::requestCancel() {
    if (!isBusy()) return; m_cancelled = true; QSaveFile marker(m_run + "/cancel"); if (marker.open(QIODevice::WriteOnly)) { marker.write("cancel"); marker.commit(); }
    const auto generation = m_generation; QTimer::singleShot(5000, this, [this, generation] { if (isBusy() && m_generation == generation && m_cancelled) m_process.kill(); });
}
void LearningReviewPanel::consume() {
    m_output += m_process.readAllStandardOutput(); if (m_output.size() > 1024 * 1024) { m_errors += "Backend protocol too long"; m_output.clear(); m_process.kill(); return; }
    qsizetype pos; while ((pos = m_output.indexOf('\n')) >= 0) { const auto o = QJsonDocument::fromJson(m_output.left(pos)).object(); m_output.remove(0, pos + 1); if (o["event"] == QJsonValue("error")) m_errors += o["message"].toString().toUtf8(); else if (o["event"] == QJsonValue("progress")) m_status->setText(o["message"].toString()); }
}
void LearningReviewPanel::finish(int code, QProcess::ExitStatus status) {
    consume(); const auto report = objectAt(m_evaluationDirectory + "/evaluation.json"); const bool ok = !m_cancelled && code == 0 && status == QProcess::NormalExit && report["format"] == QJsonValue("ZTBLD-IndependentEvaluation") && report["status"] == QJsonValue("completed");
    QString error; const bool saved = SelfLearning::writeJson(m_run + "/job.json", {{"status", ok ? "completed" : m_cancelled ? "cancelled" : "failed"}, {"evaluationPath", m_evaluationDirectory + "/evaluation.json"}, {"diagnostics", QString::fromUtf8(m_errors).right(32768)}}, error);
    m_status->setText(!saved ? error : ok ? QStringLiteral("独立测试完成。比较不代表已获审核；请查看失败原因、策略和覆盖率。") : QStringLiteral("独立测试未完成：%1").arg(QString::fromUtf8(m_errors)));
    if (ok && saved) loadRuns(); refresh(); emit busyChanged();
}
void LearningReviewPanel::loadRuns() {
    m_runs->clear(); const QDir runs(reviewDirectory() + "/test_runs");
    for (const auto &dir : runs.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time)) { const auto job = objectAt(dir.filePath() + "/job.json"); const auto path = job["evaluationPath"].toString(); const auto report = objectAt(path); if (job["status"] != QJsonValue("completed") || report["status"] != QJsonValue("completed")) continue; m_runs->addItem((report["testOnly"].toBool() ? QStringLiteral("[测试证据] ") : QStringLiteral("[独立比较] ")) + dir.fileName(), path); }
    displayRun();
}
void LearningReviewPanel::displayRun() {
    m_results->setRowCount(0); const auto report = objectAt(m_runs->currentData().toString());
    for (const auto &entry : report["models"].toArray()) { const auto model = entry.toObject(); const auto targets = model["targets"].toObject(); for (const auto &metric : SelfLearning::metrics()) { const auto t = targets[metric.key].toObject(); if (t.isEmpty()) continue; const auto base = t["baseline"].toObject(), dnn = t["model"].toObject(); QStringList failures; for (const auto &f : model["failures"].toArray()) failures << (f.isString() ? f.toString() : QString::fromUtf8(QJsonDocument(f.toObject()).toJson(QJsonDocument::Compact))); for (const auto &f : t["failures"].toArray()) failures << (f.isString() ? f.toString() : QString::fromUtf8(QJsonDocument(f.toObject()).toJson(QJsonDocument::Compact))); const QStringList cells{model["modelVersion"].toString().left(22) + " / " + QString::fromUtf8(metric.name) + " (" + metric.unit + ")", QStringLiteral("%1 / %2").arg(t["labelledSamples"].toInt()).arg(t["independentProducts"].toInt()), QString::number(100 * t["coverage"].toDouble(), 'g', 5) + "%", score(base, "MAE"), score(dnn, "MAE"), score(base, "RMSE") + " / " + score(dnn, "RMSE"), score(base, "maxAbsoluteError") + " / " + score(dnn, "maxAbsoluteError"), failures.join("; ")}; const int row = m_results->rowCount(); m_results->insertRow(row); for (int col = 0; col < cells.size(); ++col) { auto *item = new QTableWidgetItem(cells[col]); item->setToolTip(cells[col]); m_results->setItem(row, col, item); } } }
    refresh();
}
