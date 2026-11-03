#include "SelfLearningDataset.h"
#include "SchemeStore.h"
#include "LearningWorkspace.h"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QDebug>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTableWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QPixmap>
#include <QLockFile>
#include <QMessageBox>
#include <QTimer>
#include <QAbstractButton>
#include <QFontDatabase>
#include <QScrollArea>
#include <QScrollBar>
#include <limits>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    int failures = 0;
    const auto check = [&](bool ok, const char *why) {
        if (!ok) { qCritical() << why; ++failures; }
    };
    using namespace SelfLearning;
    QString error;
    Sample sample;
    check(makeSample(CalcInput{}, sample, error), "Supported complete input must create a draft");
    sample.productId = "TEST-001";
    sample.source = "Factory test report";
    sample.testedOn = QDate(2026, 10, 23);
    sample.measured[0] = 530.0;
    auto json = encode(sample);
    Sample restored;
    check(decode(json, restored, error) && restored.measured[0] == 530.0
              && !restored.measured[1].has_value(), "Missing labels must survive as missing, not zero");
    check(restored.baseline[0] == 524.0 && restored.baseline[1] == 5206.0,
          "Snapshot must use the existing full engine, not a predicted result");
    CalcInput withCriteria;
    withCriteria.hasPerformanceCriteria = true;
    withCriteria.performanceCriteria.standardMode = TransformerParams::StandardMode::Custom;
    withCriteria.performanceCriteria.productModel = "SB20-M-630/10-0.4";
    withCriteria.performanceCriteria.lossStandardsManual = true;
    Sample criterionSample;
    check(makeSample(withCriteria, criterionSample, error), "Confirmed criteria fixture must calculate");
    criterionSample.productId = "CRITERIA-001";
    check(decode(encode(criterionSample), restored, error) && restored.input.hasPerformanceCriteria,
          "A real sample with optional performance criteria must round-trip without losing its snapshot");
    {
        CalcInput withAn3;
        withAn3.hasPerformanceCriteria = true;
        withAn3.performanceCriteria.productModel = QStringLiteral("SB20-M-630/10-0.4");
        withAn3.performanceCriteria.standardMode = TransformerParams::StandardMode::Custom;
        withAn3.performanceCriteria.lossStandardsManual = true;
        withAn3.performanceCriteria.noLoadCurrentStd_pct = 0.36;
        withAn3.performanceCriteria.hasNonstandardAn3 = true;
        withAn3.performanceCriteria.nonstandardAn3_pct = 0.7;
        withAn3.performanceCriteria.hasSavedCustomNoLoadCurrent = true;
        withAn3.performanceCriteria.savedCustomNoLoadCurrent_pct = 0.36;
        Sample an3Sample;
        check(makeSample(withAn3, an3Sample, error), "AN3 criteria must allow learning sample creation");
        an3Sample.productId = "AN3-COMPAT-001";
        an3Sample.source = "Factory test report";
        an3Sample.testedOn = QDate(2026,10,23);
        an3Sample.measured[0] = 530.0;
        an3Sample.purpose = "training";
        check(decode(encode(an3Sample), restored, error) && restored.input.performanceCriteria.hasNonstandardAn3
                  && std::abs(restored.input.performanceCriteria.nonstandardAn3_pct - 0.7) < 1e-9,
              "AN3 criteria must survive the learning snapshot");
        QJsonObject an3Bundle;
        check(trainingBundle({an3Sample}, an3Bundle, error)
                  && an3Bundle["inputSchema"].toObject()["properties"].toObject()["performanceCriteria"].toObject()["properties"].toObject().contains("nonstandardAn3_pct"),
              "Learning bundle must describe optional AN3 criteria");
    }
    for (const auto value : {QJsonValue("530"), QJsonValue(-1), QJsonValue(0), QJsonValue(true)}) {
        auto invalid = json;
        auto labels = invalid["measured"].toObject();
        labels["noLoadLoss_W"] = value;
        invalid["measured"] = labels;
        check(!decode(invalid, restored, error), "Loss label must reject text, booleans and nonpositive values");
    }
    sample.measured[3] = 0.0;
    check(decode(encode(sample), restored, error) && restored.measured[3] == 0.0,
          "A real zero temperature rise must remain distinct from missing");
    auto invalid = json;
    auto input = invalid["input"].toObject();
    input.remove("lvTurns"); invalid["input"] = input;
    check(!decode(invalid, restored, error), "Missing input must not silently become default design data");
    invalid = json;
    input = invalid["input"].toObject(); input["lvTurns"] = "18"; invalid["input"] = input;
    check(!decode(invalid, restored, error), "Wrong input types must not silently become defaults");
    invalid = json; invalid["engineVersion"] = "unknown";
    check(!decode(invalid, restored, error), "Unknown formula versions must not enter training data");
    invalid = json; auto base = invalid["baseline"].toObject();
    base["noLoadLoss_W"] = 999.0; invalid["baseline"] = base;
    check(!decode(invalid, restored, error), "Tampered baseline must be rejected by recalculation");
    CalcInput unsupported; unsupported.coreShape = StructureConfig::Circle;
    Sample unusable;
    check(!makeSample(unsupported, unusable, error), "Partial-preview structures cannot become training samples");
    auto infinite = sample; infinite.measured[0] = std::numeric_limits<double>::infinity();
    QTemporaryDir dir;
    const auto path = dir.filePath("samples.json");
    check(!save(path, {infinite}, error), "Nonfinite measured data must not serialize to missing");
    check(save(path, {sample}, error), "Real dataset must persist to a temporary file");
    QVector<Sample> loaded;
    check(load(path, loaded, error) && loaded.size() == 1 && loaded[0].measured[0] == 530.0,
          "Saved measured data must reload");
    QFile beforeFile(path); check(beforeFile.open(QIODevice::ReadOnly), "Fixture must open"); const auto before = beforeFile.readAll(); beforeFile.close();
    check(importDataset(path, QJsonDocument(document({sample})).toJson(), error)
              && load(path, loaded, error) && loaded.size() == 1,
          "Importing the identical sample twice must not double-count it");
    auto conflict = sample; conflict.measured[0] = 540.0;
    auto another = sample; another.id = "another-id"; another.productId = "TEST-002";
    check(!importDataset(path, QJsonDocument(document({another, conflict})).toJson(), error),
          "A conflict must reject the entire import, not partially add earlier rows");
    QFile afterFile(path); check(afterFile.open(QIODevice::ReadOnly), "Fixture must reopen");
    check(afterFile.readAll() == before, "Rejected import must leave the original bytes unchanged");
    afterFile.close();
    auto repeatedProduct = sample; repeatedProduct.id = "different-id";
    check(!save(dir.filePath("duplicate.json"), {sample, repeatedProduct}, error),
          "Same product and test date cannot leak into two samples via another scheme library");
    {
        CalcInput customInput;
        customInput.oilTopCorrection_K = 9.0;
        Sample customSample;
        check(makeSample(customInput, customSample, error), "Custom oil correction must create a learning sample");
        customSample.productId = "CUSTOM-TEMP-001";
        customSample.source = "Factory test report";
        customSample.testedOn = QDate(2026,10,23);
        customSample.measured[3] = 41.0;
        customSample.purpose = "training";
        check(decode(encode(customSample), restored, error) && restored.input.oilTopCorrection_K == 9.0,
              "Learning sample must preserve custom correction and recalculate its baseline");
        QJsonObject customBundle;
        check(trainingBundle({customSample}, customBundle, error)
                  && customBundle["inputSchema"].toObject()["properties"].toObject().contains("oilTopCorrection_K"),
              "Learning bundle must describe the optional correction input");
    }
    QJsonObject bundle;
    // Only explicit training fixtures opt in; legacy persistence tests above stay unassigned.
    sample.purpose = "training";
    check(trainingBundle({sample}, bundle, error), "A partially labelled sample must be exportable");
    auto row = bundle["rows"].toArray().first().toObject();
    check(row["residual"].toObject()["noLoadLoss_W"].toDouble() == 6.0
              && row["residual"].toObject()["loadLoss_W"].isNull()
              && row["productId"].toString() == "TEST-001"
              && bundle["datasetSha256"].toString().size() == 64,
          "Training handoff must contain literal residual6, missing-mask and product grouping/hash");
    Sample draft = sample; draft.measured = {};
    check(!trainingBundle({draft}, bundle, error), "Unlabelled drafts cannot form a fake training job");
    check(!trainingBlockReason({}).isEmpty() && !trainingBlockReason({sample}).isEmpty(),
          "Neither empty data nor labelled data can bypass the missing training backend");
    QFile corrupt(dir.filePath("corrupt.json")); check(corrupt.open(QIODevice::WriteOnly), "Corrupt fixture must open"); corrupt.write("broken"); corrupt.close();
    check(!importDataset(corrupt.fileName(), QJsonDocument(document({sample})).toJson(), error),
          "Damaged local datasets must not be treated as empty and overwritten");
    const auto repeatedPath = dir.filePath("repeated.json");
    check(importDataset(repeatedPath, QJsonDocument(document({sample, sample})).toJson(), error)
              && load(repeatedPath, loaded, error) && loaded.size() == 1,
          "Identical rows within one import must be deduplicated too");
    auto large = sample; large.source = QString(16 * 1024 * 1024, QLatin1Char('x'));
    check(!save(path, {large}, error), "A save must not create a dataset larger than the reader's limit");
    check(load(path, loaded, error) && loaded.size() == 1 && loaded[0].source == "Factory test report",
          "Oversized replacement must preserve the prior readable dataset");
    {
        auto legacy = json; legacy.remove("purpose");
        check(decode(legacy, restored, error) && restored.purpose == "unassigned",
              "Legacy v1 samples must decode as unassigned");
        check(!trainingBundle({restored}, bundle, error), "Legacy labels must never opt into training automatically");
        for (const auto value : {QJsonValue("unknown"), QJsonValue(1), QJsonValue(true), QJsonValue(QJsonValue::Null)}) {
            auto bad = json; bad["purpose"] = value;
            check(!decode(bad, restored, error), "Unknown or nonstring purpose must reject");
        }
        for (const auto &role : {"unassigned", "training", "independent_test", "excluded"}) {
            auto roleSample = sample; roleSample.purpose = role;
            check(decode(encode(roleSample), restored, error) && restored.purpose == role,
                  "Each canonical purpose must persist without a schema bump");
        }
        auto train = sample;
        auto reserve = sample; reserve.id = "reserve"; reserve.productId = "RESERVE"; reserve.purpose = "independent_test";
        auto excluded = sample; excluded.id = "excluded"; excluded.productId = "EXCLUDED"; excluded.purpose = "excluded";
        auto unassigned = sample; unassigned.id = "unassigned"; unassigned.productId = "UNASSIGNED"; unassigned.purpose = "unassigned";
        check(validateDataset({train, reserve, excluded, unassigned}, error), "Distinct products may occupy distinct roles");
        check(trainingBundle({train, reserve, excluded, unassigned}, bundle, error)
                  && bundle["rows"].toArray().size() == 1
                  && bundle["rows"].toArray().first().toObject()["purpose"] == "training",
              "Training export must select only labelled training rows");
        check(!trainingBundle({reserve, excluded, unassigned}, bundle, error), "Reserved and excluded labels cannot train");
        auto zero = train; zero.measured = {}; zero.measured[3] = 0.0;
        check(trainingBundle({zero}, bundle, error)
                  && bundle["rows"].toArray().first().toObject()["measured"].toObject()["oilTopRise_K"] == 0.0,
              "Measured zero is a training label while other labels remain null");
        check(evaluationBundle({reserve}, bundle, error)
                  && bundle["bundleUse"] == "independent_test"
                  && bundle["rows"].toArray().first().toObject()["purpose"] == "independent_test",
              "Evaluation freezes independent-test labels with purpose");
        check(!evaluationBundle({train, reserve}, bundle, error)
                  && !evaluationBundle({excluded}, bundle, error), "Evaluation must reject every non-test selection");
        auto later = train; later.id = "later"; later.productId = " test-001 "; later.testedOn = train.testedOn.addDays(1);
        check(validateDataset({train, later}, error), "Same canonical product may have multiple dates");
        later.purpose = "independent_test";
        check(!validateDataset({train, later}, error) && !trainingBundle({train, later}, bundle, error),
              "Full-list validation must reject product leakage before training filtering");
        const auto partitionPath = dir.filePath("partition.json");
        check(!save(partitionPath, {train, later}, error), "Save must reject mixed product purposes");
        check(save(partitionPath, {train}, error), "Mixed-import fixture must save");
        check(!importDataset(partitionPath, QJsonDocument(document({later})).toJson(), error)
                  && load(partitionPath, loaded, error) && loaded.size() == 1,
              "Import must validate the final merged product roles without partially appending");
        check(!updateSample(partitionPath, later, {}, error)
                  && load(partitionPath, loaded, error) && loaded.size() == 1,
              "Single update cannot append a conflicting product purpose");
        auto unknown = train; unknown.purpose = "TRAINING";
        check(!validateDataset({unknown}, error), "In-memory purposes must also be canonical");
        auto noTestLabels = reserve; noTestLabels.measured = {};
        check(!evaluationBundle({noTestLabels}, bundle, error), "An unlabelled test draft cannot freeze an evaluation job");
        later.purpose = train.purpose;
        check(save(partitionPath, {train, later, reserve}, error), "Partition transaction fixture must save");
        check(!assignProductPurpose(partitionPath, "TEST-001", "excluded", {train}, error),
              "Incomplete expected group must reject a newly added date record");
        auto stale = later; stale.source = "stale";
        check(!assignProductPurpose(partitionPath, "TEST-001", "excluded", {train, stale}, error),
              "Group assignment must compare snapshot contents as well as IDs");
        check(assignProductPurpose(partitionPath, " Test-001 ", "excluded", {later, train}, error)
                  && load(partitionPath, loaded, error) && loaded.size() == 3
                  && loaded[0].purpose == "excluded" && loaded[1].purpose == "excluded"
                  && encode(loaded[2]) == encode(reserve) && !loaded[0].measured[1],
              "Assignment must atomically change whole product only, preserving labels and unrelated products");
        check(save(partitionPath, {train, later, reserve}, error), "Reset transaction fixture");
        auto edited = train; edited.measured[0] = 541.0;
        auto editedLater = later; editedLater.measured[0] = 542.0;
        auto invalidEdit = editedLater; invalidEdit.purpose = "unknown";
        check(!updateBatch(partitionPath, {edited, invalidEdit}, {train, later}, error)
                  && load(partitionPath, loaded, error) && encode(loaded[0]) == encode(train),
              "One invalid batch replacement must reject all edits");
        check(!updateBatch(partitionPath, {edited, editedLater}, {train, stale}, error)
                  && load(partitionPath, loaded, error) && encode(loaded[0]) == encode(train),
              "One stale batch snapshot rejects all preceding edits");
        check(!updateBatch(partitionPath, {edited, edited}, {train, train}, error), "Duplicate batch IDs reject");
        check(!updateBatch(partitionPath, {edited, unassigned}, {train, unassigned}, error), "Batch cannot append missing IDs");
        check(!updateBatch(partitionPath, {edited}, {later}, error), "Batch replacement and original IDs must match");
        edited.purpose = "excluded";
        check(!updateBatch(partitionPath, {edited}, {train}, error), "Partial product role change rejects entire batch");
        edited.purpose = "training";
        check(updateBatch(partitionPath, {editedLater, edited}, {train, later}, error)
                  && load(partitionPath, loaded, error) && loaded[0].measured[0] == 541.0
                  && loaded[1].measured[0] == 542.0 && encode(loaded[2]) == encode(reserve),
              "Batch matches snapshots by ID and atomically applies all replacements");
        QLockFile held(partitionPath + ".lock");
        check(held.tryLock(), "Partition test holds writer lock");
        check(!updateBatch(partitionPath, {train}, {edited}, error)
                  && !assignProductPurpose(partitionPath, "TEST-001", "excluded", {edited, editedLater}, error),
              "Both partition mutations must respect the writer lock");
    }
    {
        const auto freezePath = dir.filePath("freeze.json");
        auto train = sample;
        auto test = sample; test.id = "freeze-test"; test.productId = "FREEZE-TEST"; test.purpose = "independent_test";
        auto other = sample; other.id = "freeze-other"; other.productId = "FREEZE-OTHER"; other.purpose = "excluded";
        const QVector<Sample> expected{train, test, other};
        check(save(freezePath, expected, error), "Freeze fixture must persist");
        QJsonObject frozen;
        check(freezeDatasetBundle(freezePath, {other, train, test}, {}, false, frozen, error)
                  && frozen["bundleUse"] == "training" && frozen["rows"].toArray().size() == 1
                  && frozen["rows"].toArray().first().toObject()["id"] == train.id,
              "Freeze must compare the whole dataset by ID regardless of expected row order");
        check(freezeDatasetBundle(freezePath, expected, {test.id}, true, frozen, error)
                  && frozen["bundleUse"] == "independent_test" && frozen["rows"].toArray().size() == 1
                  && frozen["rows"].toArray().first().toObject()["id"] == test.id,
              "Evaluation freeze must select fresh persisted test rows");
        const auto rejected = [&](const QString &file, const QVector<Sample> &snapshot,
                                  const QStringList &ids, bool evaluation, const char *why) {
            frozen = QJsonObject{{"staleOutput", true}};
            const bool ok = freezeDatasetBundle(file, snapshot, ids, evaluation, frozen, error);
            check(!ok && frozen.isEmpty() && !error.isEmpty(), why);
        };
        rejected(freezePath, expected, {train.id}, false, "Training freeze rejects selectors and clears output");
        rejected(freezePath, expected, {}, true, "Evaluation freeze requires explicit selectors");
        rejected(freezePath, expected, {test.id, test.id}, true, "Evaluation freeze rejects duplicate selectors");
        rejected(freezePath, expected, {test.id, "absent"}, true, "Evaluation freeze rejects any missing selector atomically");
        rejected(freezePath, expected, {train.id}, true, "Evaluation freeze rejects training-purpose rows");
        rejected(freezePath, expected, {other.id}, true, "Evaluation freeze rejects excluded rows");
        rejected(dir.filePath("absent-freeze.json"), expected, {}, false, "Freeze cannot fall back to cached data for a missing file");
        rejected(dir.filePath("absent-empty-freeze.json"), {}, {}, false, "Missing files are not empty persisted datasets");
        for (const auto &purpose : {"independent_test", "excluded"}) {
            auto changed = train; changed.purpose = purpose;
            check(save(freezePath, {changed, test, other}, error), "Other window persists product role change");
            rejected(freezePath, expected, {}, false, "Cached training role cannot bypass a persisted reservation or exclusion");
        }
        auto changed = other; changed.source = "new report metadata";
        check(save(freezePath, {train, test, changed}, error), "Other window changes an unselected row");
        rejected(freezePath, expected, {test.id}, true, "Freeze compares unselected row contents too");
        check(save(freezePath, {train, test}, error), "Other window deletes a row");
        rejected(freezePath, expected, {}, false, "Freeze rejects deleted dataset rows");
        check(save(freezePath, expected, error), "Restore complete dataset");
        rejected(freezePath, {train, test}, {}, false, "Freeze rejects newly added dataset rows");
        rejected(freezePath, {train, test, other, other}, {}, false, "Duplicate expected IDs cannot disguise an incomplete snapshot");
        {
            QLockFile held(freezePath + ".lock");
            check(held.tryLock(), "Freeze fixture acquires dataset writer lock");
            rejected(freezePath, expected, {}, false, "Freeze must hold the writer lock and clear output on lock failure");
        }
        check(freezeDatasetBundle(freezePath, expected, {}, false, frozen, error),
              "Freeze succeeds after the writer releases its lock");
    }
    auto upper = sample; upper.productId = "ABC";
    auto lower = sample; lower.productId = "abc"; lower.id = "lower"; lower.testedOn = QDate(2026, 10, 24);
    check(trainingBundle({upper, lower}, bundle, error), "Repeated tests of one product may exist on different dates");
    const auto grouped = bundle["rows"].toArray();
    check(bundle["splitGroupKey"].toString() == "productGroupId"
              && grouped[0].toObject()["productGroupId"].toString() == "abc"
              && grouped[1].toObject()["productGroupId"].toString() == "abc",
          "Case variants of one product must share one training/validation group");
    check(bundle["inputSchema"].toObject()["id"].toString() == "ztbld-calcinput-learning-v1"
              && bundle["inputSchema"].toObject()["properties"].toObject().contains("lvTurns"),
          "Backend handoff must carry an explicit versioned feature-input schema");
    check(bundle["inputSchema"].toObject()["properties"].toObject().contains("performanceCriteria")
              && !bundle["inputSchema"].toObject()["required"].toArray().contains("performanceCriteria"),
          "The input contract must allow optional persisted performance criteria, not forbid them");
    {
        const auto lockPath = dir.filePath("locked.json");
        check(save(lockPath, {sample}, error), "Lock fixture must start readable");
        QLockFile lock(lockPath + ".lock");
        check(lock.tryLock(), "Fixture must hold the dataset writer lock");
        check(!importDataset(lockPath, QJsonDocument(document({another})).toJson(), error),
              "Import must not write while another application owns the dataset transaction");
    }
    {
        LearningWorkspace workspace(dir.filePath("ui/samples.json"));
        const auto button = [&](const char *name) { return workspace.findChild<QPushButton *>(name); };
        check(button("saveSample") && !button("saveSample")->isEnabled(), "Cannot save without a draft");
        check(button("startTraining") && !button("startTraining")->isEnabled()
                  && !button("activateModel")->isEnabled() && !button("rollbackModel")->isEnabled(),
              "No backend/model must keep training, activation and rollback unavailable");
        workspace.setScheme(CalcInput{}, "Default verified scheme");
        button("newSample")->click();
        workspace.findChild<QLineEdit *>("productId")->setText("UI-001");
        workspace.findChild<QLineEdit *>("sampleSource")->setText("Report-001");
        workspace.findChild<QLineEdit *>("measured_0")->setText("530");
        button("saveSample")->click();
        QVector<Sample> fromUi;
        check(load(dir.filePath("ui/samples.json"), fromUi, error) && fromUi.size() == 1
                  && fromUi[0].measured[0] == 530.0 && !fromUi[0].measured[1],
              "GUI must save optional measured values through the real repository");
        workspace.findChild<QLineEdit *>("measured_0")->clear();
        check(!button("exportTemplate")->isEnabled(),
              "An edited measurement must require save before template export, not export stale labels");
        button("saveSample")->click();
        check(load(dir.filePath("ui/samples.json"), fromUi, error) && fromUi.size() == 1
                  && !fromUi[0].measured[0] && !button("exportTraining")->isEnabled(),
              "Editing a saved sample to missing must update, not duplicate or keep stale labels");
        workspace.findChild<QLineEdit *>("measured_0")->setText("nonsense");
        button("saveSample")->click();
        check(load(dir.filePath("ui/samples.json"), fromUi, error) && !fromUi[0].measured[0],
              "Invalid GUI measurement must not overwrite the persisted sample");
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (dialog && dialog->button(QMessageBox::Discard)) dialog->button(QMessageBox::Discard)->click();
        });
        check(workspace.confirmDiscard() && workspace.findChild<QLineEdit *>("measured_0")->text().isEmpty(),
              "Discard must actually restore editor contents even if the next operation aborts");
        auto *metricsTable = workspace.findChild<QTableWidget *>("learningMetrics");
        metricsTable->item(0, 3)->setToolTip("previous model version");
        button("reloadSamples")->click();
        check(metricsTable->item(0, 3)->toolTip().isEmpty(), "Clearing the active sample must clear stale model provenance too");
        LearningWorkspace reopened(dir.filePath("ui/samples.json"));
        check(reopened.findChild<QTableWidget *>("sampleList")->rowCount() == 1,
              "Reopening the workspace must restore the local sample list");
        auto precise = fromUi[0]; precise.measured[0] = 530.123456789123;
        check(save(dir.filePath("precision.json"), {precise}, error), "Precision fixture must persist");
        LearningWorkspace precision(dir.filePath("precision.json"));
        precision.findChild<QTableWidget *>("sampleList")->setCurrentCell(0, 0);
        precision.findChild<QLineEdit *>("sampleSource")->setText("Metadata-only edit");
        precision.findChild<QPushButton *>("saveSample")->click();
        QVector<Sample> exact;
        check(load(dir.filePath("precision.json"), exact, error) && exact.size() == 1
                  && exact[0].measured[0] == 530.123456789123,
              "Metadata-only edits must not round measured labels or change their residuals");
        {
            QLockFile held(dir.filePath("precision.json") + ".lock");
            check(held.tryLock(), "GUI lock fixture must acquire the transaction lock");
            precision.findChild<QLineEdit *>("measured_0")->setText("540");
            precision.findChild<QPushButton *>("saveSample")->click();
            check(load(dir.filePath("precision.json"), exact, error) && exact.size() == 1
                      && exact[0].measured[0] == 530.123456789123,
                  "GUI saving must respect the same transaction lock as imports");
        }
        auto stale = exact[0];
        const auto oldSnapshot = encode(stale);
        precision.findChild<QPushButton *>("saveSample")->click();
        stale.measured[0] = 550.0;
        check(!updateSample(dir.filePath("precision.json"), stale, oldSnapshot, error)
                  && load(dir.filePath("precision.json"), exact, error) && exact[0].measured[0] == 540.0,
              "Stale editors must not overwrite another window's newer measurement");
        const auto capture = qEnvironmentVariable("ZTBLD_LEARNING_IMAGE");
        if (!capture.isEmpty()) {
            const int font = QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
            if (font >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(), 10));
            QFile style(":/styles/ztf_theme.qss");
            if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
            QTabWidget tabs;
            auto *preview = new LearningWorkspace(dir.filePath("ui/samples.json"), &tabs);
            tabs.addTab(preview, QStringLiteral("学习样本"));
            tabs.addTab(preview->trainingPage(), QStringLiteral("训练与模型"));
            preview->setScheme(CalcInput{}, QStringLiteral("SB20-M-630/10-0.4"));
            preview->findChild<QTableWidget *>("sampleList")->setCurrentCell(0, 0);
            tabs.resize(1100, 820); tabs.show(); app.processEvents();
            check(tabs.grab().save(capture), "Offscreen sample screenshot must save");
            tabs.setCurrentIndex(1); app.processEvents();
            check(tabs.grab().save(capture + ".training.png"), "Offscreen training screenshot must save");
            auto *scroll = preview->trainingPage()->findChild<QScrollArea *>();
            if (scroll) {
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); app.processEvents();
                check(tabs.grab().save(capture + ".review.png"), "Offscreen enterprise review screenshot must save");
                auto *testList = preview->trainingPage()->findChild<QWidget *>("independentTestSamples");
                if (testList) scroll->ensureWidgetVisible(testList, 30, 80);
                app.processEvents(); check(tabs.grab().save(capture + ".comparison.png"), "Offscreen comparison screenshot must save");
            }
        }
    }
    qInfo() << "self-learning dataset failures:" << failures;
    return failures ? 1 : 0;
}
