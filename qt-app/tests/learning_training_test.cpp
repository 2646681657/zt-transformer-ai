#include "LearningTrainingPanel.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <QComboBox>
#include <QCheckBox>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QTableWidget>
#include <QListWidget>
#include "LearningModelReview.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    LearningTrainingPanel panel(dir.path());
    int failures = 0;
    auto check = [&](bool ok, const char *why) { if (!ok) { ++failures; qCritical() << why; } };
    check(!panel.isBusy(), "Panel starts idle without spawning a process");
    auto *train = panel.findChild<QPushButton *>("startTraining");
    check(train && !train->isEnabled(), "Unconfigured panel cannot start training");
    panel.setRuntime(dir.filePath("missing-python.exe"), dir.filePath("missing-trainer.py"));
    panel.findChild<QPushButton *>("probeBackend")->click();
    check(!panel.isBusy() && !panel.findChild<QLabel *>("backendStatus")->text().isEmpty(),
          "Missing runtime must report failure without a hanging process");
    check(!panel.findChild<QPushButton *>("activateModel")->isEnabled()
              && !panel.findChild<QPushButton *>("rollbackModel")->isEnabled(),
          "Experimental models have no production activation or rollback");
    check(!panel.findChild<QPushButton *>("previewPrediction")->isEnabled(),
          "No model/sample means no independent prediction");
    check(panel.findChild<QListWidget *>("independentTestSamples") != nullptr,
          "Independent comparison must explicitly select frozen test products");
    check(panel.findChild<QTableWidget *>("approvalPolicyTargets") != nullptr,
          "Enterprise thresholds must have an editor separate from training validation");
    auto *evaluate = panel.findChild<QPushButton *>("evaluateModels");
    check(evaluate && !evaluate->isEnabled(), "No samples/models/runtime means no independent evaluation");
    auto *defaultPredict = panel.findChild<QPushButton *>("predictApprovedDefault");
    check(defaultPredict && !defaultPredict->isEnabled(), "No approved default means no approved correction");
    auto *policy = panel.findChild<QTableWidget *>("approvalPolicyTargets");
    if (policy) for (int r = 0; r < policy->rowCount(); ++r)
        check(policy->item(r, 2) && policy->item(r, 2)->text().isEmpty(), "Do not invent enterprise accuracy thresholds");
    {
        QTemporaryDir preciseDir; QString error;
        const double threshold = 123.12345678901234;
        QJsonObject target{{"maxMAE", threshold}, {"maxRMSE", threshold}, {"maxAbsoluteError", threshold}, {"minProducts", 1}, {"notWorseThanBaseline", false}};
        QJsonObject precisePolicy{{"format", "ZTBLD-DisplayApprovalPolicy"}, {"version", 1}, {"policyVersion", "precision-regression"}, {"source", "Temporary precision fixture, not enterprise threshold"}, {"confirmed", true}, {"targets", QJsonObject{{"noLoadLoss_W", target}}}};
        check(LearningReview::savePolicy(preciseDir.path() + "/review", precisePolicy, error), "Precision fixture policy persists");
        LearningTrainingPanel reopened(preciseDir.path());
        auto *table = reopened.findChild<QTableWidget *>("approvalPolicyTargets");
        check(table && table->item(0, 2)->text().toDouble() == threshold, "Reopening enterprise thresholds must preserve exact double precision");
    }
    {
        LearningTrainingPanel reopened(dir.path());
        check(reopened.findChild<QLineEdit *>("trainingPython")->text() == dir.filePath("missing-python.exe"),
              "Explicit runtime configuration must persist and reopen");
    }
    const QString badJob = dir.path() + "/jobs/incomplete";
    QString writeError;
    SelfLearning::writeJson(badJob + "/job.json", {{"status", "completed"}}, writeError);
    SelfLearning::writeJson(badJob + "/model.json", {{"format", "ZTBLD-ResidualModel"}, {"version", 1},
        {"status", "experimental"}, {"productionEligible", false}, {"testOnly", true}, {"modelVersion", "incomplete"}}, writeError);
    {
        LearningTrainingPanel damaged(dir.path());
        check(damaged.findChild<QComboBox *>("modelHistory")->count() == 0,
              "Incomplete model or missing report cannot appear as completed history");
    }
    const auto python = qEnvironmentVariable("ZTBLD_TRAINING_PYTHON");
    const auto script = qEnvironmentVariable("ZTBLD_TRAINER_SCRIPT");
    if (app.arguments().contains("--integration") && !python.isEmpty() && !script.isEmpty()) {
        auto wait = [&](int ms) {
            QElapsedTimer timer; timer.start();
            while (panel.isBusy() && timer.elapsed() < ms) { app.processEvents(); QThread::msleep(10); }
            return !panel.isBusy();
        };
        QVector<SelfLearning::Sample> samples; QString error;
        for (int i = 0; i < 10; ++i) {
            SelfLearning::Sample s;
            check(SelfLearning::makeSample(CalcInput{}, s, error), "Fixture input must be real engine output");
            s.productId = QStringLiteral("SYNTHETIC-%1").arg(i); s.source = "SYNTHETIC TEST ONLY";
            s.purpose="training";
            s.measured[0] = s.baseline[0] + 6.0; samples.append(s);
        }
        check(SelfLearning::save(dir.filePath("samples.json"),samples,error),"Integration dataset must actually persist before freezing jobs");
        panel.setDataset(samples); panel.setPreviewSample(SelfLearning::encode(samples[0]));
        const auto localScript = dir.filePath("trainer.py");
        check(QFile::copy(script, localScript), "Integration backend script must be isolated before path-removal testing");
        panel.setRuntime(python, localScript);
        panel.findChild<QPushButton *>("probeBackend")->click();
        check(!panel.findChild<QPushButton *>("browseTrainingPython")->isEnabled()
                  && !panel.findChild<QPushButton *>("browseTrainingScript")->isEnabled(),
              "Runtime file pickers must remain disabled while probing");
        check(wait(20000), "Real PyTorch probe must finish without blocking event processing");
        check(train->isEnabled(), "A successful backend probe and five groups must permit training");
        panel.findChild<QCheckBox *>("testOnlyTraining")->setChecked(true);
        auto reserved=samples; for(auto &sample:reserved) sample.purpose="independent_test";
        check(SelfLearning::save(dir.filePath("samples.json"),reserved,error),"Other-window reserved-purpose fixture persists");
        const auto priorJobs=QDir(dir.path()+"/jobs").entryList(QDir::Dirs|QDir::NoDotAndDotDot);
        train->click();
        check(!panel.isBusy() && QDir(dir.path()+"/jobs").entryList(QDir::Dirs|QDir::NoDotAndDotDot)==priorJobs,"Stale training cache must not create or launch a job after another window reserves its products");
        if(panel.isBusy()) {panel.requestCancel();wait(15000);}
        check(SelfLearning::save(dir.filePath("samples.json"),samples,error),"Restore temporary training roles");
        panel.setDataset(samples);
        train->click(); check(wait(30000), "Real CPU training must finish");
        auto *history = panel.findChild<QComboBox *>("modelHistory");
        check(history->count() == 1, "Only a completed job with real model/report may enter model history");
        if (history->count()) {
            QFile model(history->currentData().toString() + "/model.json");
            check(model.open(QIODevice::ReadOnly), "Completed model must exist");
            const auto object = QJsonDocument::fromJson(model.readAll()).object();
            check(object["testOnly"] == QJsonValue(true) && object["productionEligible"] == QJsonValue(false),
                  "Synthetic UI job must remain permanently nonproduction");
            bool predicted = false;
            const auto predictionConnection = QObject::connect(&panel, &LearningTrainingPanel::predictionReady, &panel, [&](const QJsonObject &p, bool testOnly) {
                predicted = p["noLoadLoss_W"].isDouble() && testOnly;
            });
            panel.findChild<QPushButton *>("previewPrediction")->click();
            check(wait(20000) && predicted, "Safe model reload must return a separate test-only prediction");
            bool invalidated = false;
            const auto invalidationConnection = QObject::connect(&panel, &LearningTrainingPanel::predictionInvalidated, &panel, [&] { invalidated = true; });
            QMetaObject::invokeMethod(history, "currentIndexChanged", Qt::DirectConnection, Q_ARG(int, 0));
            check(invalidated, "Changing/reselecting model must invalidate prior displayed predictions");
            QObject::disconnect(predictionConnection); QObject::disconnect(invalidationConnection);
            auto *policyTable = panel.findChild<QTableWidget *>("approvalPolicyTargets");
            policyTable->item(0, 0)->setCheckState(Qt::Checked);
            for (int col = 2; col < 6; ++col) policyTable->item(0, col)->setText("1");
            panel.findChild<QLineEdit *>("approvalPolicyVersion")->setText("temporary-mechanics-policy-A");
            panel.findChild<QLineEdit *>("approvalPolicySource")->setText("Temporary UI regression fixture, not enterprise criteria");
            panel.findChild<QCheckBox *>("approvalPolicyConfirmed")->setChecked(true);
            bool stalePrediction = false;
            auto staleConnection = QObject::connect(&panel, &LearningTrainingPanel::predictionReady, &panel,
                [&](const QJsonObject &) { stalePrediction = true; });
            panel.findChild<QPushButton *>("previewPrediction")->click();
            panel.findChild<QPushButton *>("saveApprovalPolicy")->click();
            check(wait(20000) && !stalePrediction, "Registry change must invalidate even an in-flight experimental prediction");
            QObject::disconnect(staleConnection);
            auto policyB = LearningReview::policy(dir.path() + "/review", error);
            policyB["policyVersion"] = "temporary-mechanics-policy-B";
            check(LearningReview::savePolicy(dir.path() + "/review", policyB, error), "Second-window policy fixture must save");
            panel.setDataset(samples); // Refresh reads current persistent policy, not just the old editor.
            check(panel.findChild<QLabel *>("modelApprovalBlockers")->text().contains(QStringLiteral("审核标准已被其他窗口更改")),
                  "Approval must visibly block when displayed policy differs from persisted policy");
            auto *reloadPolicy = panel.findChild<QPushButton *>("reloadApprovalPolicy");
            check(reloadPolicy != nullptr, "Stale policy needs explicit reload without silently discarding edits");
            if (reloadPolicy) { reloadPolicy->click(); check(panel.findChild<QLineEdit *>("approvalPolicyVersion")->text() == policyB["policyVersion"].toString(), "Explicit policy reload must show the actual current criteria"); }
            auto independent = samples;
            for (int i = 0; i < independent.size(); ++i) { independent[i].id += "-independent"; independent[i].productId = QStringLiteral("SYNTHETIC-INDEPENDENT-%1").arg(i); independent[i].purpose="independent_test"; }
            check(SelfLearning::save(dir.filePath("samples.json"),independent,error),"Reserved independent test products persist before frozen evaluation");
            panel.setDataset(independent);
            auto *testSamples = panel.findChild<QListWidget *>("independentTestSamples");
            for (int i = 0; i < testSamples->count(); ++i) testSamples->item(i)->setCheckState(Qt::Checked);
            auto *compare = panel.findChild<QPushButton *>("evaluateModels");
            check(compare->isEnabled(), "Separate frozen products and completed model permit an explicit test");
            auto excluded=independent; for(auto &sample:excluded) sample.purpose="excluded";
            check(SelfLearning::save(dir.filePath("samples.json"),excluded,error),"Other-window excluded test fixture persists");
            const auto priorRuns=QDir(dir.path()+"/review/test_runs").entryList(QDir::Dirs|QDir::NoDotAndDotDot);
            compare->click();
            check(!panel.isBusy()&&QDir(dir.path()+"/review/test_runs").entryList(QDir::Dirs|QDir::NoDotAndDotDot)==priorRuns,
                  "Stale evaluation cache must not freeze products excluded by another window");
            if(panel.isBusy()) {panel.requestCancel();wait(15000);}
            check(SelfLearning::save(dir.filePath("samples.json"),independent,error),"Restore independent test roles");
            compare->click(); check(wait(30000), "Independent CPU evaluation must finish while UI remains responsive");
            check(panel.findChild<QComboBox *>("independentTestHistory")->count() == 1,
                  "Successful immutable evaluation must enter comparison history");
            check(panel.findChild<QTableWidget *>("independentTestResults")->rowCount() == SelfLearning::MetricCount,
                  "Independent report must expose all target denominators including missing labels");
            check(!panel.findChild<QPushButton *>("approveDisplayModel")->isEnabled(),
                  "Synthetic evidence without enterprise policy must never gain approval");
            check(SelfLearning::save(dir.filePath("samples.json"),samples,error),"Restore persisted training sample fixtures");
            panel.setDataset(samples);
        }
        train->click(); panel.requestCancel();
        check(wait(15000) && history->count() == 1, "Cancellation cannot publish a second model");
        if (history->count()) {
            const auto path = history->currentData().toString() + "/model.json";
            QFile f(path); check(f.open(QIODevice::ReadOnly), "Valid artifact must open for mutation fixture");
            const auto original = QJsonDocument::fromJson(f.readAll()).object(); f.close();
            for (const auto key : {"normalization", "bounds"}) {
                auto mutated = original; auto targets = mutated["models"].toObject();
                auto target = targets["noLoadLoss_W"].toObject(); target.remove(key);
                targets["noLoadLoss_W"] = target; mutated["models"] = targets;
                check(SelfLearning::writeJson(path, mutated, error), "Mutation must persist only in temporary test directory");
                panel.findChild<QPushButton *>("reloadModelHistory")->click();
                check(history->count() == 0, "Model missing essential inference fields cannot enter completed history");
            }
            check(SelfLearning::writeJson(path, original, error), "Temporary valid model fixture must restore");
            panel.findChild<QPushButton *>("reloadModelHistory")->click();
        }
        if (history->count()) {
            check(QFile::remove(history->currentData().toString() + "/report.json"), "Fixture report must be removable");
            panel.findChild<QPushButton *>("reloadModelHistory")->click();
            check(history->count() == 0, "Deleted/corrupt validation report must invalidate previously completed history");
        }
        check(QFile::remove(localScript), "Only the temporary backend script fixture is removed");
        train->click();
        for (const auto &job : QDir(dir.path() + "/jobs").entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile record(job.filePath() + "/job.json");
            if (record.open(QIODevice::ReadOnly)) check(QJsonDocument::fromJson(record.readAll()).object()["status"] != QJsonValue("running"),
                "Runtime removed after probe must not leave a durable running training job");
        }
    } else if (app.arguments().contains("--integration")) {
        qWarning() << "SKIP: set ZTBLD_TRAINING_PYTHON and ZTBLD_TRAINER_SCRIPT for real CPU integration";
        return 77;
    }
    qInfo() << "training UI failures:" << failures;
    return failures ? 1 : 0;
}
