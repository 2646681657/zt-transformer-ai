#include "LearningModelReview.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QTemporaryDir>
#include <cstdio>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static void write(const QString &path, QJsonObject o) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly))
    qFatal("fixture write failed");
  f.write(QJsonDocument(o).toJson());
}
static QString sha(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256)
      .toHex();
}
static QJsonArray vector(int n, double v) {
  QJsonArray a;
  while (n--)
    a.append(v);
  return a;
}
static QJsonArray matrix(int rows, int cols) {
  QJsonArray a;
  while (rows--)
    a.append(vector(cols, 0));
  return a;
}
// Deliberately hand-authored transition fixture. This proves registry
// mechanics, never model accuracy, actual provenance or production approval.
struct Fixture {
  QString model, report, bundle, evaluation;
  QJsonObject m, r, b, ev;
  explicit Fixture(QString root) {
    model = root + "/model.json";
    report = root + "/report.json";
    bundle = root + "/test.json";
    evaluation = root + "/evaluation.json";
    QJsonArray features;
    for (const auto &s :
         QString("capacity_kVA hvRated_kV lvRated_kV coreDiameter_mm "
                 "coreStraight_mm stackFactor steelThickness_mm hvBareWidth_mm "
                 "hvBareThick_mm lvTurns lvFoilThick_mm lvFoilWidth_mm "
                 "mainDuctWidth_mm refFluxDens_T hvCopperWire lvCopperFoil")
             .split(' '))
      features.append(s);
    QJsonObject input{{"coreShape", 2}, {"yokeShape", 1}, {"hvCoilFormIdx", 1}};
    QJsonObject props;
    QJsonArray required;
    for (const auto &v : features)
      input[v.toString()] = v.toString().startsWith("hvCopper") ||
                                    v.toString().startsWith("lvCopper")
                                ? QJsonValue(true)
                                : QJsonValue(1);
    for (auto it = input.begin(); it != input.end(); ++it) {
      props[it.key()] =
          QJsonObject{{"type", it.value().isBool() ? "boolean" : "number"}};
      required.append(it.key());
    }
    QJsonObject schema{{"id", "ztbld-calcinput-learning-v1"},
                       {"type", "object"},
                       {"properties", props},
                       {"required", required},
                       {"additionalProperties", false}};
    QJsonObject validation{{"baselineMAE", 1},
                           {"modelMAE", 0},
                           {"RMSE", 0},
                           {"bestEpoch", 1},
                           {"validationSamples", 1}};
    QJsonObject weights{{"0.weight", matrix(32, 16)}, {"0.bias", vector(32, 0)},
                        {"2.weight", matrix(16, 32)}, {"2.bias", vector(16, 0)},
                        {"4.weight", matrix(1, 16)},  {"4.bias", vector(1, 0)}};
    QJsonObject t{
        {"weights", weights},
        {"normalization", QJsonObject{{"mean", vector(16, 1)},
                                      {"scale", vector(16, 1)},
                                      {"targetMean", 0},
                                      {"targetScale", 1}}},
        {"bounds", QJsonObject{{"min", vector(16, 0)}, {"max", vector(16, 2)}}},
        {"trainGroups", QJsonArray{"train"}},
        {"validationGroups", QJsonArray{"validation"}},
        {"trainSampleIds", QJsonArray{"train-id"}},
        {"validationSampleIds", QJsonArray{"validation-id"}},
        {"labelledSamples", 2},
        {"validation", validation}};
    QJsonObject context{
        {"coreShape", 2}, {"yokeShape", 1}, {"hvCoilFormIdx", 1}};
    m = {{"format", "ZTBLD-ResidualModel"},
         {"version", 1},
         {"status", "experimental"},
         {"productionEligible", false},
         {"testOnly", false},
         {"modelVersion", root},
         {"scope", "oil-foil-stacked-ellipse-tyoke-dual-cylinder-50hz-75c"},
         {"engineVersion", "sb20-default-sheet12-learning-v1"},
         {"featureOrder", features},
         {"architecture", QJsonArray{16, 32, 16, 1}},
         {"inputSchema", schema},
         {"fixedContext", context},
         {"models", QJsonObject{{"noLoadLoss_W", t}}}};
    r = {{"status", "completed"},
         {"productionEligible", false},
         {"testOnly", false},
         {"modelVersion", root},
         {"modelPath", model},
         {"validation", QJsonObject{{"noLoadLoss_W", validation}}}};
    QJsonObject values, labels, residual;
    QJsonArray targets;
    const QStringList keys{"noLoadLoss_W",    "loadLoss_W",
                           "impedance_pct",   "oilTopRise_K",
                           "hvWindingRise_K", "lvWindingRise_K"};
    for (int i = 0; i < keys.size(); ++i) {
      values[keys[i]] = 1;
      labels[keys[i]] = 1;
      residual[keys[i]] = 0;
      targets.append(QJsonObject{{"key", keys[i]},
                                 {"unit", i < 2    ? "W"
                                          : i == 2 ? "%"
                                                   : "K"}});
    }
    QJsonObject row{{"id", "test-id"},
                    {"productId", "independent"},
                    {"productGroupId", "independent"},
                    {"source", "explicit registry fixture"},
                    {"schemeName", "fixture"},
                    {"testedOn", "2026-10-23"},
                    {"scope", m["scope"]},
                    {"engineVersion", m["engineVersion"]},
                    {"input", input},
                    {"baseline", values},
                    {"measured", labels},
                    {"residual", residual}};
    b = {{"format", "ZTBLD-ResidualTrainingBundle"},
         {"version", 1},
         {"scope", m["scope"]},
         {"engineVersion", m["engineVersion"]},
         {"testOnly", false},
         {"datasetSha256", QString(64, 'a')},
         {"inputSchema", schema},
         {"splitGroupKey", "productGroupId"},
         {"residualDefinition", "measured-baseline"},
         {"targets", targets},
         {"rows", QJsonArray{row}}};
    auto train = b;
    auto rows = QJsonArray{};
    for (const auto &group : QStringList{"train", "validation"}) {
      auto x = row;
      x["id"] = group + "-id";
      x["productId"] = group;
      x["productGroupId"] = group;
      rows.append(x);
    }
    train["rows"] = rows;
    write(root + "/bundle.json", train);
    QJsonObject evidence{{"bundleFileSha256", sha(root + "/bundle.json")},
                         {"sourceDatasetSha256", QString(64, 'a')}};
    m["evidence"] = evidence;
    r["evidence"] = evidence;
    write(root + "/job.json",
          {{"status", "completed"}, {"productionEligible", false}});
    QJsonObject stats{{"MAE", 0}, {"RMSE", 0}, {"maxAbsoluteError", 0}};
    QJsonObject target{{"labelledSamples", 1},
                       {"independentProducts", 1},
                       {"predictedSamples", 1},
                       {"coverage", 1},
                       {"failures", QJsonArray{}},
                       {"baseline", stats},
                       {"model", stats}};
    QJsonObject entry{{"modelPath", model},
                      {"reportPath", report},
                      {"modelVersion", root},
                      {"scope", m["scope"]},
                      {"engineVersion", m["engineVersion"]},
                      {"testOnly", false},
                      {"eligible", true},
                      {"failures", QJsonArray{}},
                      {"targets", QJsonObject{{"noLoadLoss_W", target}}}};
    ev = {{"format", "ZTBLD-IndependentEvaluation"},
          {"version", 1},
          {"status", "completed"},
          {"testOnly", false},
          {"bundlePath", bundle},
          {"models", QJsonArray{entry}}};
    sync();
  }
  void sync() {
    write(model, m);
    write(report, r);
    write(bundle, b);
    auto a = ev["models"].toArray();
    auto x = a[0].toObject();
    x["modelSha256"] = sha(model);
    x["reportSha256"] = sha(report);
    a[0] = x;
    ev["models"] = a;
    ev["bundleSha256"] = sha(bundle);
    write(evaluation, ev);
  }
};

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir tmp;
  int failures = 0;
  auto check = [&](bool ok, const char *why) {
    if (!ok) {
      fprintf(stderr, "FAIL: %s\n", why);
      ++failures;
    }
  };
  QString e;
  using namespace LearningReview;
  check(policy(tmp.path(), e).isEmpty(), "No default enterprise policy");
  check(!approve(tmp.path(), "missing", "missing", "reviewer", "reason", e),
        "Missing evidence rejected");
  QJsonObject target{{"maxMAE", 1},
                     {"maxRMSE", 2},
                     {"maxAbsoluteError", 3},
                     {"minProducts", 1},
                     {"notWorseThanBaseline", true}};
  QJsonObject p{{"format", "ZTBLD-DisplayApprovalPolicy"},
                {"version", 1},
                {"policyVersion", "fixture-v1"},
                {"source", "Explicit test fixture, not production proof"},
                {"confirmed", true},
                {"targets", QJsonObject{{"noLoadLoss_W", target}}}};
  check(savePolicy(tmp.path(), p, e), "Confirmed policy persists");
  check(policy(tmp.path(), e) == p, "Policy survives reopen");
  auto bad = p;
  bad["targets"] = QJsonObject{{"unknown", target}};
  check(!savePolicy(tmp.path(), bad, e), "Unknown metric rejected");
  bad = p;
  target.remove("minProducts");
  bad["targets"] = QJsonObject{{"noLoadLoss_W", target}};
  check(!savePolicy(tmp.path(), bad, e), "No default product threshold");
  check(policy(tmp.path(), e) == p, "Rejected write preserves policy");
  check(!activate(tmp.path(), "missing", e), "Unapproved model rejected");
  check(!rollback(tmp.path(), e), "No arbitrary rollback fallback");
  check(deactivate(tmp.path(), "local audit fixture", e),
        "Deactivation audited even when empty");
  check(activeModel(tmp.path(), e).isEmpty(), "Deactivation clears default");
  check(state(tmp.path(), e)["events"].toArray().size() == 2,
        "Policy and deactivation share persisted audit");
  Fixture f(tmp.filePath("a"));
  // Simulate another window saving a different policy after this one rendered.
  QTemporaryDir guarded;
  const auto renderedHash = QCryptographicHash::hash(
      QJsonDocument(p).toJson(QJsonDocument::Compact),
      QCryptographicHash::Sha256).toHex();
  check(savePolicy(guarded.path(), p, e), "Initialize guarded policy");
  auto concurrentPolicy = p;
  concurrentPolicy["policyVersion"] = "concurrent-window-policy";
  check(savePolicy(guarded.path(), concurrentPolicy, e), "Other window changes policy");
  const auto concurrentState = state(guarded.path(), e);
  check(!savePolicy(guarded.path(), p, e, renderedHash) &&
            e.contains("stale", Qt::CaseInsensitive),
        "Stale displayed policy cannot overwrite current policy");
  check(state(guarded.path(), e) == concurrentState,
        "Stale policy save preserves entire snapshot");
  check(!approve(guarded.path(), f.model, f.evaluation, "r", "reason", e,
                 renderedHash) && e.contains("stale", Qt::CaseInsensitive),
        "Stale displayed policy cannot authorize approval");
  check(state(guarded.path(), e) == concurrentState,
        "Stale approval preserves approvals, audit and active model");
  const auto currentHash = QCryptographicHash::hash(
      QJsonDocument(concurrentPolicy).toJson(QJsonDocument::Compact),
      QCryptographicHash::Sha256).toHex();
  check(approve(guarded.path(), f.model, f.evaluation, "r", "reason", e,
                currentHash), "Matching displayed policy permits approval");
  check(savePolicy(guarded.path(), p, e, currentHash),
        "Matching displayed policy permits policy replacement");
  check(blockers(tmp.path(), f.model, f.evaluation).isEmpty(),
        "Complete explicit transition fixture meets gates");
  check(!approve(tmp.path(), f.model, f.evaluation, "", "reason", e),
        "Reviewer required");
  check(approve(tmp.path(), f.model, f.evaluation, "local reviewer",
                "fixture only", e),
        "Approve audited fixture");
  check(activate(tmp.path(), f.model, e),
        "Approved fixture activates for display");
  check(activeModel(tmp.path(), e) == f.model,
        "Reopened active default validated");
  check(canActivate(tmp.path(), f.model, e) && !canRollback(tmp.path(), e),
        "UI capability checks use current valid evidence");
  const auto approvedState = state(tmp.path(), e);
  // A previous-engine model must fail the version gate even with fresh hashes
  // and a saved approval, rather than merely fail because evidence changed.
  const auto currentModel = f.m;
  f.m["engineVersion"] = "sb20-sheet11-learning-v1";
  f.sync();
  auto legacyState = approvedState;
  auto legacyApprovals = legacyState["approvals"].toArray();
  auto legacyApproval = legacyApprovals[0].toObject();
  legacyApproval["engineVersion"] = f.m["engineVersion"];
  legacyApproval["modelSha256"] = sha(f.model);
  legacyApproval["evaluationSha256"] = sha(f.evaluation);
  legacyApprovals[0] = legacyApproval;
  legacyState["approvals"] = legacyApprovals;
  write(tmp.filePath("registry/registry.json"), legacyState);
  check(!approve(tmp.path(), f.model, f.evaluation, "reviewer", "legacy", e) &&
            e.contains("Model scope/version mismatch"),
        "Previous engine model cannot receive a new approval");
  check(!canActivate(tmp.path(), f.model, e) &&
            e.contains("Model scope/version mismatch"),
        "Previous engine approval is not eligible for activation");
  check(!activate(tmp.path(), f.model, e) &&
            e.contains("Model scope/version mismatch"),
        "Saved previous engine approval cannot activate");
  check(activeModel(tmp.path(), e).isEmpty() &&
            e.contains("Model scope/version mismatch"),
        "Previously active engine model is invalidated on reopen");
  check(state(tmp.path(), e) == legacyState,
        "Rejected legacy approval and activation preserve registry");
  f.m = currentModel;
  f.sync();
  write(tmp.filePath("registry/registry.json"), approvedState);
  QLockFile held(tmp.filePath("registry/registry.json.lock"));
  check(held.tryLock(0), "Acquire competing writer lock");
  check(!deactivate(tmp.path(), "must not persist", e),
        "Competing writer fails closed");
  held.unlock();
  check(state(tmp.path(), e) == approvedState,
        "Lock failure preserves whole audit and active snapshot");
#ifdef Q_OS_WIN
  // Deny replacement of the destination to exercise a real QSaveFile commit
  // failure.
  const auto registryPath = tmp.filePath("registry/registry.json");
  HANDLE heldSnapshot = CreateFileW(
      reinterpret_cast<LPCWSTR>(registryPath.utf16()), GENERIC_READ,
      FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  check(heldSnapshot != INVALID_HANDLE_VALUE,
        "Hold snapshot against atomic replacement");
  if (heldSnapshot != INVALID_HANDLE_VALUE) {
    check(!deactivate(tmp.path(), "failed snapshot commit", e),
          "Atomic replacement failure reported");
    CloseHandle(heldSnapshot);
    check(state(tmp.path(), e) == approvedState,
          "Commit failure preserves approvals, events and active state");
  }
#endif
  auto revoked = approvedState;
  auto revokedApprovals = revoked["approvals"].toArray();
  auto revokedApproval = revokedApprovals[0].toObject();
  revokedApproval["revoked"] = true;
  revokedApprovals[0] = revokedApproval;
  revoked["approvals"] = revokedApprovals;
  write(tmp.filePath("registry/registry.json"), revoked);
  check(!canActivate(tmp.path(), f.model, e) &&
            activeModel(tmp.path(), e).isEmpty() && !canRollback(tmp.path(), e),
        "Revoked approvals cannot activate or roll back");
  write(tmp.filePath("registry/registry.json"), approvedState);
  QFile changed(f.model);
  check(changed.open(QIODevice::Append), "Open model for byte tampering");
  changed.write(" ");
  changed.close();
  check(!canActivate(tmp.path(), f.model, e) &&
            activeModel(tmp.path(), e).isEmpty(),
        "Byte change with identical JSON invalidates bound approval");
  f.sync();
  auto jobPath = QFileInfo(f.model).dir().filePath("job.json");
  write(jobPath, {{"status", "running"}, {"productionEligible", false}});
  check(!canActivate(tmp.path(), f.model, e) &&
            activeModel(tmp.path(), e).isEmpty(),
        "Running job cannot masquerade as completed report");
  write(jobPath, {{"status", "completed"}, {"productionEligible", false}});
  auto originalModel = f.m;
  auto mt = f.m["models"].toObject();
  auto nt = mt["noLoadLoss_W"].toObject();
  auto weights = nt["weights"].toObject();
  weights["0.weight"] = matrix(1, 16);
  nt["weights"] = weights;
  mt["noLoadLoss_W"] = nt;
  f.m["models"] = mt;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Malformed tensor rejected even with fresh report hashes");
  f.m = originalModel;
  f.sync();
  auto originalBundle = f.b;
  auto leakingRows = f.b["rows"].toArray();
  auto leaking = leakingRows[0].toObject();
  leaking["productId"] = "TRAIN";
  leaking["productGroupId"] = "train";
  leakingRows[0] = leaking;
  f.b["rows"] = leakingRows;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Canonical product identity prevents same-product leakage");
  f.b = originalBundle;
  f.sync();
  auto outsideRows = f.b["rows"].toArray();
  auto outside = outsideRows[0].toObject();
  auto outsideInput = outside["input"].toObject();
  outsideInput["capacity_kVA"] = 3;
  outside["input"] = outsideInput;
  outsideRows[0] = outside;
  f.b["rows"] = outsideRows;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Forged eligibility cannot hide features outside training bounds");
  f.b = originalBundle;
  f.sync();
  Fixture g(tmp.filePath("b"));
  QTemporaryDir noPolicy;
  check(!approve(noPolicy.path(), g.model, g.evaluation, "r", "reason", e),
        "Complete evidence still requires a saved enterprise policy");
  check(!canRollback(tmp.path(), e),
        "Merely existing model files are not rollback history");
  check(approve(tmp.path(), g.model, g.evaluation, "reviewer", "fixture only",
                e) &&
            activate(tmp.path(), g.model, e),
        "Second audited transition");
  check(rollback(tmp.path(), e) && activeModel(tmp.path(), e) == f.model,
        "Rollback selects valid historical approval");
  check(canRollback(tmp.path(), e),
        "Read-only rollback gate agrees with historical transition");
  f.m["testOnly"] = true;
  f.sync();
  check(activeModel(tmp.path(), e).isEmpty(),
        "Changed artifact invalidates active default");
  check(!approve(tmp.path(), f.model, f.evaluation, "reviewer", "reason", e),
        "Test-only model rejected");
  f.m["testOnly"] = false;
  f.sync();
  auto original = f.b;
  auto stableState = state(tmp.path(), e);
  for (const auto &field : QStringList{"testOnly", "synthetic", "source"}) {
    auto rawRows = f.b["rows"].toArray();
    auto raw = rawRows[0].toObject();
    raw[field] =
        field == "source"
            ? QJsonValue("SYNTHETIC fixture hidden by false report flags")
            : QJsonValue(true);
    rawRows[0] = raw;
    f.b["rows"] = rawRows;
    f.sync();
    check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
          "Raw row flags override purported eligible report");
    check(state(tmp.path(), e) == stableState,
          "Rejected approval changes neither active state nor audit");
    f.b = original;
  }
  auto provenanceModel = f.m;
  auto trained = f.m["models"].toObject();
  auto trainedTarget = trained["noLoadLoss_W"].toObject();
  trainedTarget["trainGroups"] = QJsonArray{"invented"};
  trained["noLoadLoss_W"] = trainedTarget;
  f.m["models"] = trained;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Training group provenance is checked against frozen raw training "
        "bundle");
  f.m = provenanceModel;
  f.sync();
  auto unchangedEvaluation = f.ev;
  f.ev["productionEligible"] = true;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Future production-eligibility flags cannot promote display evidence");
  f.ev = unchangedEvaluation;
  f.b["synthetic"] = true;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Synthetic evidence rejected");
  f.b = original;
  auto rows = f.b["rows"].toArray();
  auto row = rows[0].toObject();
  row["measured"] = QJsonObject{};
  rows[0] = row;
  f.b["rows"] = rows;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Forged report cannot hide missing labels");
  f.b = original;
  auto evOriginal = f.ev;
  auto models = f.ev["models"].toArray();
  auto entry = models[0].toObject();
  auto ts = entry["targets"].toObject();
  auto t = ts["noLoadLoss_W"].toObject();
  t["predictedSamples"] = 0;
  ts["noLoadLoss_W"] = t;
  entry["targets"] = ts;
  models[0] = entry;
  f.ev["models"] = models;
  f.sync();
  check(!approve(tmp.path(), f.model, f.evaluation, "r", "reason", e),
        "Coverage flag cannot hide failed raw count");
  f.ev = evOriginal;
  f.sync();
  const auto originalPolicy = p;
  check(savePolicy(tmp.path(), p, e) && canActivate(tmp.path(), f.model, e) &&
            activeModel(tmp.path(), e) == f.model,
        "Saving identical policy preserves current approval and active model");
  p["policyVersion"] = "fixture-v2";
  check(savePolicy(tmp.path(), p, e) && activeModel(tmp.path(), e).isEmpty() &&
            !rollback(tmp.path(), e),
        "Policy changes invalidate approvals and rollback");
  check(savePolicy(tmp.path(), originalPolicy, e), "Restore exact original policy");
  check(activeModel(tmp.path(), e).isEmpty() &&
            state(tmp.path(), e)["activeModel"].toString().isEmpty(),
        "P1 to P2 to P1 never resurrects old active model");
  check(!canActivate(tmp.path(), f.model, e) &&
            !canActivate(tmp.path(), g.model, e),
        "Restoring policy content cannot revive old approvals");
  check(!canRollback(tmp.path(), e) && !rollback(tmp.path(), e),
        "Restoring policy content cannot revive historical rollback");
  check(approve(tmp.path(), f.model, f.evaluation, "reviewer", "fresh review", e) &&
            canActivate(tmp.path(), f.model, e) && activate(tmp.path(), f.model, e),
        "Fresh approval under restored policy permits activation");
  QFile obstruction(tmp.filePath("obstruction"));
  check(obstruction.open(QIODevice::WriteOnly), "Create obstruction");
  obstruction.write("x");
  obstruction.close();
  check(!savePolicy(obstruction.fileName(), p, e), "Write failure reported");
  fprintf(stderr, "model_review_test: %d failure(s)\n", failures);
  return failures ? 1 : 0;
}
