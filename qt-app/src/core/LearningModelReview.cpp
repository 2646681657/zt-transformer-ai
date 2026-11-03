#include "LearningModelReview.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <functional>
#include <limits>

namespace LearningReview {
namespace {
const QString scope = "oil-foil-stacked-ellipse-tyoke-dual-cylinder-50hz-75c",
              engine = "sb20-sheet11-learning-v1";
const QStringList metrics{"noLoadLoss_W", "loadLoss_W",      "impedance_pct",
                          "oilTopRise_K", "hvWindingRise_K", "lvWindingRise_K"};
const QStringList features =
    QString("capacity_kVA hvRated_kV lvRated_kV coreDiameter_mm "
            "coreStraight_mm stackFactor steelThickness_mm hvBareWidth_mm "
            "hvBareThick_mm lvTurns lvFoilThick_mm lvFoilWidth_mm "
            "mainDuctWidth_mm refFluxDens_T hvCopperWire lvCopperFoil")
        .split(' ');
void need(bool ok, QString why) {
  if (!ok)
    throw why;
}
bool num(QJsonValue v) { return v.isDouble() && std::isfinite(v.toDouble()); }
bool nonneg(QJsonValue v) { return num(v) && v.toDouble() >= 0; }
bool integer(QJsonValue v, int minimum = 1) {
  return num(v) && v.toDouble() >= minimum && v.toDouble() <= INT_MAX &&
         std::floor(v.toDouble()) == v.toDouble();
}
QString absolute(QString p) {
  need(!p.trimmed().isEmpty(), "Missing artifact path");
  return QDir::cleanPath(QFileInfo(p).absoluteFilePath());
}
QString digest(QByteArray b) {
  return QString::fromLatin1(
      QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex());
}
QString hashPolicy(QJsonObject p) {
  return digest(QJsonDocument(p).toJson(QJsonDocument::Compact));
}
QJsonObject read(QString p, QString *sha = nullptr) {
  QFile f(p);
  need(f.open(QIODevice::ReadOnly), "Cannot read: " + p);
  need(f.size() > 0 && f.size() <= 16 * 1024 * 1024,
       "Invalid artifact size: " + p);
  auto raw = f.readAll();
  QJsonParseError e;
  auto doc = QJsonDocument::fromJson(raw, &e);
  need(e.error == QJsonParseError::NoError && doc.isObject(),
       "Invalid JSON: " + p);
  if (sha)
    *sha = digest(raw);
  return doc.object();
}
void policyCheck(QJsonObject p) {
  need(p["format"] == QJsonValue("ZTBLD-DisplayApprovalPolicy") &&
           p["version"] == QJsonValue(1),
       "No supported enterprise policy");
  need(p["confirmed"] == QJsonValue(true) &&
           !p["policyVersion"].toString().trimmed().isEmpty() &&
           !p["source"].toString().trimmed().isEmpty(),
       "Policy requires confirmation, version and source");
  auto ts = p["targets"].toObject();
  need(!ts.isEmpty(), "Policy targets required");
  for (auto it = ts.begin(); it != ts.end(); ++it) {
    auto t = it.value().toObject();
    need(metrics.contains(it.key()), "Unknown policy metric");
    for (auto k : {"maxMAE", "maxRMSE", "maxAbsoluteError"})
      need(nonneg(t[k]), "Missing/invalid policy threshold");
    need(integer(t["minProducts"]) && t["notWorseThanBaseline"].isBool(),
         "Explicit product count/baseline rule required");
  }
}
void flags(QJsonObject o) {
  need(!o.contains("productionEligible") ||
           o["productionEligible"] == QJsonValue(false),
       "Production eligibility cannot authorize independent display evidence");
  need(!o.contains("testOnly") || o["testOnly"] == QJsonValue(false),
       "Test-only evidence forbidden");
  need(!o.contains("synthetic") || o["synthetic"] == QJsonValue(false),
       "Synthetic evidence forbidden");
  need(!o["source"].toString().contains("synthetic", Qt::CaseInsensitive),
       "Synthetic source forbidden");
}
void schema(QJsonValue v, QJsonObject s) {
  auto type = s["type"].toString();
  if (type == "object") {
    need(v.isObject() && s["properties"].isObject() &&
             s["required"].isArray() &&
             s["additionalProperties"] == QJsonValue(false),
         "Invalid input schema");
    auto o = v.toObject(), props = s["properties"].toObject();
    for (auto k : s["required"].toArray())
      need(k.isString() && o.contains(k.toString()), "Missing required input");
    for (auto it = o.begin(); it != o.end(); ++it) {
      need(props[it.key()].isObject(), "Unknown input");
      schema(it.value(), props[it.key()].toObject());
    }
  } else if (type == "array") {
    auto items = s["itemsByIndex"].toArray();
    need(v.isArray() && integer(s["length"], 0) &&
             v.toArray().size() == s["length"].toInt() &&
             items.size() == v.toArray().size(),
         "Array shape mismatch");
    for (int i = 0; i < items.size(); ++i)
      schema(v.toArray()[i], items[i].toObject());
  } else
    need(type == "number"    ? num(v) && std::abs(v.toDouble()) <= 1000000
         : type == "boolean" ? v.isBool()
                             : type == "string" && v.isString(),
         "Invalid scalar/schema");
}
QJsonObject context(QJsonObject o) {
  for (auto k : features)
    o.remove(k);
  for (auto k : {"materialPrices", "steelPricePerKg", "useCustomSteelPrice",
                 "steelPriceGrade", "performanceCriteria", "craftConstraints",
                 "legacyUnitLossRecordText", "legacyCraftCoefRecordText"})
    o.remove(k);
  return o;
}
QSet<QString> strings(QJsonValue v, bool canonical = false) {
  need(v.isArray() && !v.toArray().isEmpty(), "Missing provenance array");
  QSet<QString> out;
  for (auto x : v.toArray()) {
    auto s = x.toString();
    need(!s.trimmed().isEmpty() && !out.contains(s) &&
             (!canonical || s == s.trimmed().toCaseFolded()),
         "Invalid provenance");
    out.insert(s);
  }
  return out;
}
void tensor(QJsonValue v, int rows, int cols = 0, bool positive = false) {
  need(v.isArray() && v.toArray().size() == rows, "Tensor shape mismatch");
  for (auto x : v.toArray())
    if (cols)
      tensor(x, cols);
    else
      need(num(x) &&
               std::abs(x.toDouble()) <= std::numeric_limits<float>::max() &&
               (!positive || x.toDouble() > 0),
           "Invalid tensor value");
}
QJsonObject bundle(QString p, QString &sha, QJsonObject m) {
  auto b = read(p, &sha);
  flags(b);
  need(b["format"] == QJsonValue("ZTBLD-ResidualTrainingBundle") &&
           b["version"] == QJsonValue(1) && b["scope"] == scope &&
           b["engineVersion"] == engine &&
           b["splitGroupKey"] == QJsonValue("productGroupId") &&
           b["residualDefinition"] == QJsonValue("measured-baseline"),
       "Invalid frozen bundle metadata");
  auto source = b["datasetSha256"].toString();
  need(source.size() == 64, "Missing source digest");
  for (auto c : source)
    need(QString("0123456789abcdef").contains(c), "Invalid source digest");
  need(b["inputSchema"] == m["inputSchema"], "Bundle schema mismatch");
  auto ts = b["targets"].toArray();
  need(ts.size() == 6, "Incomplete target metadata");
  for (int i = 0; i < 6; ++i)
    need(ts[i].toObject()["key"] == metrics[i] &&
             ts[i].toObject()["unit"] == QJsonValue(i < 2    ? "W"
                                                    : i == 2 ? "%"
                                                             : "K"),
         "Invalid target metadata");
  auto rows = b["rows"].toArray();
  need(!rows.isEmpty(), "Empty bundle");
  QSet<QString> ids, dates;
  for (auto value : rows) {
    need(value.isObject(), "Invalid row");
    auto r = value.toObject();
    flags(r);
    auto id = r["id"].toString(),
         group = r["productId"].toString().trimmed().toCaseFolded(),
         date = r["testedOn"].toString();
    need(!id.trimmed().isEmpty() && !ids.contains(id) && !group.isEmpty() &&
             r["productGroupId"] == group &&
             QDate::fromString(date, Qt::ISODate).isValid() &&
             !dates.contains(group + "/" + date),
         "Invalid sample/group/date provenance");
    ids.insert(id);
    dates.insert(group + "/" + date);
    need(r["scope"] == scope && r["engineVersion"] == engine &&
             r["schemeName"].isString(),
         "Sample scope mismatch");
    schema(r["input"], b["inputSchema"].toObject());
    auto input = r["input"].toObject();
    need(input["coreShape"] == QJsonValue(2) &&
             input["yokeShape"] == QJsonValue(1) &&
             input["hvCoilFormIdx"] == QJsonValue(1) &&
             context(input) == m["fixedContext"].toObject(),
         "Fixed physical context mismatch");
    for (int i = 0; i < 16; ++i)
      need(i < 14 ? num(input[features[i]]) : input[features[i]].isBool(),
           "Invalid feature");
    auto base = r["baseline"].toObject(), labels = r["measured"].toObject(),
         residual = r["residual"].toObject();
    need(base.size() == 6 && labels.size() == 6 && residual.size() == 6,
         "Incomplete raw labels/baseline");
    bool labelled = false;
    for (int i = 0; i < 6; ++i) {
      auto k = metrics[i];
      need(base.contains(k) && labels.contains(k) && residual.contains(k) &&
               num(base[k]) && base[k].toDouble() > 0,
           "Invalid baseline");
      if (labels[k].isNull())
        need(residual[k].isNull(), "Residual without label");
      else {
        labelled = true;
        need(nonneg(labels[k]) && (i >= 3 || labels[k].toDouble() > 0) &&
                 !r["source"].toString().trimmed().isEmpty() &&
                 num(residual[k]) &&
                 std::abs(residual[k].toDouble() - labels[k].toDouble() +
                          base[k].toDouble()) <= 1e-7,
             "Invalid label/residual/provenance");
      }
    }
    need(labelled, "Unlabelled sample");
  }
  return b;
}
QJsonObject binding(QJsonObject p, QString mp, QString ep) {
  policyCheck(p);
  mp = absolute(mp);
  ep = absolute(ep);
  QString mh, rh, eh, bh, th, jh;
  auto m = read(mp, &mh);
  auto rp = QFileInfo(mp).dir().filePath("report.json");
  auto r = read(rp, &rh);
  auto ev = read(ep, &eh);
  flags(m);
  flags(r);
  flags(ev);
  need(m["format"] == QJsonValue("ZTBLD-ResidualModel") &&
           m["version"] == QJsonValue(1) &&
           m["status"] == QJsonValue("experimental") &&
           m["productionEligible"] == QJsonValue(false) &&
           m["testOnly"] == QJsonValue(false),
       "Invalid display model metadata");
  need(m["scope"] == scope && m["engineVersion"] == engine &&
           !m["modelVersion"].toString().trimmed().isEmpty(),
       "Model scope/version mismatch");
  QJsonArray fs;
  for (auto f : features)
    fs.append(f);
  need(m["featureOrder"] == QJsonValue(fs) &&
           m["architecture"] == QJsonValue(QJsonArray{16, 32, 16, 1}) &&
           m["fixedContext"].isObject() &&
           m["inputSchema"].toObject()["id"] ==
               QJsonValue("ztbld-calcinput-learning-v1"),
       "Model structure/schema mismatch");
  need(r["status"] == QJsonValue("completed") &&
           r["testOnly"] == QJsonValue(false) &&
           r["productionEligible"] == QJsonValue(false) &&
           r["modelVersion"] == m["modelVersion"] &&
           absolute(r["modelPath"].toString()) == mp,
       "Sibling report mismatch/incomplete");
  auto job = read(QFileInfo(mp).dir().filePath("job.json"), &jh);
  flags(job);
  need(job["status"] == QJsonValue("completed") &&
           job["productionEligible"] == QJsonValue(false),
       "Training job not completed");
  auto train = bundle(QFileInfo(mp).dir().filePath("bundle.json"), th, m);
  need(m["evidence"].isObject() && m["evidence"] == r["evidence"] &&
           m["evidence"].toObject()["bundleFileSha256"] == th &&
           m["evidence"].toObject()["sourceDatasetSha256"] ==
               train["datasetSha256"],
       "Training evidence digest mismatch");
  need(ev["format"] == QJsonValue("ZTBLD-IndependentEvaluation") &&
           ev["version"] == QJsonValue(1) &&
           ev["status"] == QJsonValue("completed") &&
           ev["testOnly"] == QJsonValue(false),
       "Invalid independent evaluation");
  auto b = bundle(absolute(ev["bundlePath"].toString()), bh, m);
  need(ev["bundleSha256"] == bh, "Independent bundle digest mismatch");
  QJsonObject entry;
  int matches = 0;
  need(ev["models"].isArray(), "Missing evaluated models");
  for (auto x : ev["models"].toArray())
    if (absolute(x.toObject()["modelPath"].toString()) == mp) {
      entry = x.toObject();
      ++matches;
    }
  need(matches == 1, "Missing/duplicate evaluated model");
  flags(entry);
  need(entry["modelSha256"] == mh && entry["reportSha256"] == rh &&
           absolute(entry["reportPath"].toString()) == absolute(rp) &&
           entry["modelVersion"] == m["modelVersion"] &&
           entry["scope"] == scope && entry["engineVersion"] == engine &&
           entry["testOnly"] == QJsonValue(false) &&
           entry["eligible"] == QJsonValue(true) &&
           entry["failures"].isArray() && entry["failures"].toArray().isEmpty(),
       "Evaluation model binding/eligibility mismatch");
  auto models = m["models"].toObject(), validation = r["validation"].toObject();
  need(!models.isEmpty() && validation.size() == models.size(),
       "Missing trained targets/validation");
  QSet<QString> allGroups, allIds;
  for (auto it = models.begin(); it != models.end(); ++it) {
    need(metrics.contains(it.key()) && it.value().isObject(),
         "Unknown trained target");
    auto t = it.value().toObject(), v = t["validation"].toObject();
    need(!v.isEmpty() && validation[it.key()] == v && integer(v["bestEpoch"]) &&
             integer(v["validationSamples"]),
         "Sibling validation mismatch");
    for (auto k : {"baselineMAE", "modelMAE", "RMSE"})
      need(nonneg(v[k]), "Invalid validation statistics");
    auto n = t["normalization"].toObject(), bounds = t["bounds"].toObject(),
         w = t["weights"].toObject();
    tensor(n["mean"], 16);
    tensor(n["scale"], 16, 0, true);
    tensor(bounds["min"], 16);
    tensor(bounds["max"], 16);
    need(num(n["targetMean"]) && num(n["targetScale"]) &&
             n["targetScale"].toDouble() > 0,
         "Invalid target normalization");
    for (int i = 0; i < 16; ++i)
      need(bounds["min"].toArray()[i].toDouble() <=
               bounds["max"].toArray()[i].toDouble(),
           "Reversed bounds");
    need(w.size() == 6, "Invalid weight keys");
    tensor(w["0.weight"], 32, 16);
    tensor(w["0.bias"], 32);
    tensor(w["2.weight"], 16, 32);
    tensor(w["2.bias"], 16);
    tensor(w["4.weight"], 1, 16);
    tensor(w["4.bias"], 1);
    auto tr = strings(t["trainGroups"], true),
         va = strings(t["validationGroups"], true),
         ti = strings(t["trainSampleIds"]),
         vi = strings(t["validationSampleIds"]);
    need(!tr.intersects(va) && !ti.intersects(vi),
         "Training/validation leakage");
    QSet<QString> rawGroups, rawIds;
    int labelled = 0, validationCount = 0;
    for (auto row : train["rows"].toArray()) {
      auto o = row.toObject();
      if (o["measured"].toObject()[it.key()].isNull())
        continue;
      auto group = o["productGroupId"].toString(), id = o["id"].toString();
      need((tr.contains(group) && ti.contains(id)) ||
               (va.contains(group) && vi.contains(id)),
           "Training provenance omitted/misassigned");
      rawGroups.insert(group);
      rawIds.insert(id);
      ++labelled;
      if (va.contains(group))
        ++validationCount;
    }
    need(rawGroups == (tr | va) && rawIds == (ti | vi) &&
             integer(t["labelledSamples"]) &&
             labelled == t["labelledSamples"].toInt() &&
             validationCount == v["validationSamples"].toInt(),
         "Incomplete training provenance");
    allGroups.unite(tr | va);
    allIds.unite(ti | vi);
  }
  for (auto row : b["rows"].toArray())
    need(!allGroups.contains(row.toObject()["productGroupId"].toString()) &&
             !allIds.contains(row.toObject()["id"].toString()),
         "Independent test leaks training/validation");
  auto targets = p["targets"].toObject();
  for (auto it = targets.begin(); it != targets.end(); ++it) {
    auto key = it.key();
    need(models.contains(key), "Policy target not trained: " + key);
    auto t = entry["targets"].toObject()[key].toObject(),
         limit = it.value().toObject();
    int labelled = 0;
    QSet<QString> products;
    double sum = 0, squares = 0, maximum = 0;
    const auto bounds = models[key].toObject()["bounds"].toObject();
    for (auto row : b["rows"].toArray()) {
      auto o = row.toObject();
      // Own the value: an auto QJsonValueRef would outlive this temporary object.
      const QJsonValue value = o["measured"].toObject()[key];
      if (value.isNull())
        continue;
      const auto input = o["input"].toObject();
      for (int i = 0; i < features.size(); ++i) {
        const auto v = input[features[i]];
        const double x = v.isBool() ? (v.toBool() ? 1. : 0.) : v.toDouble();
        need(x >= bounds["min"].toArray()[i].toDouble() &&
                 x <= bounds["max"].toArray()[i].toDouble(),
             "Independent feature outside training bounds: " + key + "/" +
                 features[i]);
      }
      ++labelled;
      products.insert(o["productGroupId"].toString());
      double err =
          std::abs(value.toDouble() - o["baseline"].toObject()[key].toDouble());
      sum += err;
      squares += err * err;
      maximum = qMax(maximum, err);
    }
    need(labelled > 0 && products.size() >= limit["minProducts"].toInt() &&
             integer(t["labelledSamples"]) &&
             t["labelledSamples"].toInt() == labelled &&
             integer(t["predictedSamples"]) &&
             t["predictedSamples"].toInt() == labelled &&
             integer(t["independentProducts"]) &&
             t["independentProducts"].toInt() == products.size() &&
             t["coverage"] == QJsonValue(1) && t["failures"].isArray() &&
             t["failures"].toArray().isEmpty(),
         "Incomplete raw labels/products/prediction coverage: " + key);
    auto base = t["baseline"].toObject(), result = t["model"].toObject();
    QStringList names{"MAE", "RMSE", "maxAbsoluteError"},
        limits{"maxMAE", "maxRMSE", "maxAbsoluteError"};
    double expected[] = {sum / labelled, std::sqrt(squares / labelled),
                         maximum};
    for (int i = 0; i < 3; ++i) {
      need(nonneg(base[names[i]]) && nonneg(result[names[i]]) &&
               std::abs(base[names[i]].toDouble() - expected[i]) <=
                   1e-7 * qMax(1., expected[i]),
           "Invalid/raw baseline statistics mismatch");
      need(result[names[i]].toDouble() <= limit[limits[i]].toDouble() &&
               (!limit["notWorseThanBaseline"].toBool() ||
                result[names[i]].toDouble() <= base[names[i]].toDouble()),
           "Policy error threshold exceeded: " + key);
    }
  }
  return {{"modelPath", mp},
          {"evaluationPath", ep},
          {"modelSha256", mh},
          {"reportSha256", rh},
          {"bundleSha256", bh},
          {"trainingBundleSha256", th},
          {"jobSha256", jh},
          {"evaluationSha256", eh},
          {"policySha256", hashPolicy(p)},
          {"scope", scope},
          {"engineVersion", engine}};
}
QString registry(QString d) {
  return QDir(d).filePath("registry/registry.json");
}
QJsonObject load(QString d) {
  if (!QFileInfo::exists(registry(d)))
    return {{"format", "ZTBLD-DisplayApprovalRegistry"},
            {"version", 1},
            {"events", QJsonArray{}},
            {"approvals", QJsonArray{}},
            {"activeModel", QString()}};
  auto s = read(registry(d));
  need(s["format"] == QJsonValue("ZTBLD-DisplayApprovalRegistry") &&
           s["version"] == QJsonValue(1) && s["events"].isArray() &&
           s["approvals"].isArray() && s["activeModel"].isString(),
       "Invalid registry snapshot");
  return s;
}
void event(QJsonObject &s, QString action, QJsonObject details = {}) {
  details["action"] = action;
  details["timestamp"] =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  details["usage"] = "independent-display-only";
  auto a = s["events"].toArray();
  a.append(details);
  s["events"] = a;
}
bool transaction(QString d, QString &error,
                 std::function<void(QJsonObject &)> fn) {
  error.clear();
  try {
    need(!d.trimmed().isEmpty() &&
             QDir().mkpath(QFileInfo(registry(d)).absolutePath()),
         "Cannot create registry directory");
    QLockFile lock(registry(d) + ".lock");
    need(lock.tryLock(0), "Approval registry locked");
    auto s = load(d);
    fn(s);
    QSaveFile f(registry(d));
    f.setDirectWriteFallback(false);
    need(f.open(QIODevice::WriteOnly), "Cannot open registry snapshot");
    auto bytes = QJsonDocument(s).toJson();
    need(f.write(bytes) == bytes.size() && f.commit(),
         "Cannot atomically commit registry snapshot");
    return true;
  } catch (const QString &e) {
    error = e;
    return false;
  }
}
void approved(QJsonObject s, QString model) {
  model = absolute(model);
  auto approvals = s["approvals"].toArray();
  for (auto it = approvals.end(); it != approvals.begin();) {
    auto a = (--it)->toObject();
    if (a["modelPath"] != model)
      continue;
    need(a["revoked"] != QJsonValue(true), "Approval revoked");
    need(!a["reviewer"].toString().trimmed().isEmpty() &&
             !a["reason"].toString().trimmed().isEmpty(),
         "Missing local reviewer/reason");
    auto current =
        binding(s["policy"].toObject(), model, a["evaluationPath"].toString());
    for (auto j = current.begin(); j != current.end(); ++j)
      need(a[j.key()] == j.value(), "Approval stale: " + j.key());
    return;
  }
  throw QString("No current approval");
}
QString rollbackCandidate(QJsonObject s) {
  auto events = s["events"].toArray();
  for (auto it = events.end(); it != events.begin();) {
    auto ev = (--it)->toObject();
    if (ev["action"] != QJsonValue("activated") &&
        ev["action"] != QJsonValue("rolled-back"))
      continue;
    auto m = ev["modelPath"].toString();
    if (m.isEmpty() || m == s["activeModel"].toString())
      continue;
    try {
      approved(s, m);
      return m;
    } catch (const QString &) {
    }
  }
  throw QString("No valid historically active approval for rollback");
}
} // namespace
QJsonObject state(const QString &d, QString &e) {
  e.clear();
  try {
    return load(d);
  } catch (const QString &why) {
    e = why;
    return {};
  }
}
QJsonObject policy(const QString &d, QString &e) {
  return state(d, e)["policy"].toObject();
}
bool savePolicy(const QString &d, QJsonObject p, QString &e,
                QString expectedPolicySha256) {
  return transaction(d, e, [&](QJsonObject &s) {
    need(expectedPolicySha256.isEmpty() ||
             hashPolicy(s["policy"].toObject()) == expectedPolicySha256,
         "Displayed approval policy is stale; reload before saving policy");
    policyCheck(p);
    if (s["policy"].toObject() != p) {
      // Invalidation is durable: restoring an earlier policy's bytes must not
      // restore the approvals granted before an intervening policy change.
      auto approvals = s["approvals"].toArray();
      int invalidated = 0;
      for (int i = 0; i < approvals.size(); ++i) {
        auto approval = approvals[i].toObject();
        if (approval["revoked"] == QJsonValue(true))
          continue;
        approval["revoked"] = true;
        approval["revocationReason"] = "Approval policy changed; fresh review required";
        approval["revokedAt"] =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        approvals[i] = approval;
        ++invalidated;
      }
      s["approvals"] = approvals;
      s["activeModel"] = QString();
      if (invalidated)
        event(s, "policy-approvals-revoked", {{"count", invalidated},
              {"previousPolicySha256", hashPolicy(s["policy"].toObject())},
              {"replacementPolicySha256", hashPolicy(p)}});
    }
    s["policy"] = p;
    event(s, "policy-saved", {{"policySha256", hashPolicy(p)}});
  });
}
QStringList blockers(const QString &d, QString m, QString ev) {
  try {
    binding(load(d)["policy"].toObject(), m, ev);
    return {};
  } catch (const QString &e) {
    return {e};
  }
}
bool approve(const QString &d, QString m, QString ev, QString reviewer,
             QString reason, QString &e, QString expectedPolicySha256) {
  return transaction(d, e, [&](QJsonObject &s) {
    need(expectedPolicySha256.isEmpty() ||
             hashPolicy(s["policy"].toObject()) == expectedPolicySha256,
         "Displayed approval policy is stale; reload before approving model");
    need(!reviewer.trimmed().isEmpty() && !reason.trimmed().isEmpty(),
         "Local reviewer and reason required (not authentication)");
    auto a = binding(s["policy"].toObject(), m, ev);
    a["reviewer"] = reviewer.trimmed();
    a["reason"] = reason.trimmed();
    a["timestamp"] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    a["usage"] = "independent-display-only";
    auto approvals = s["approvals"].toArray();
    approvals.append(a);
    s["approvals"] = approvals;
    event(s, "approved", a);
  });
}
bool canActivate(const QString &d, QString m, QString &e) {
  e.clear();
  try {
    approved(load(d), m);
    return true;
  } catch (const QString &why) {
    e = why;
    return false;
  }
}
bool canRollback(const QString &d, QString &e) {
  e.clear();
  try {
    rollbackCandidate(load(d));
    return true;
  } catch (const QString &why) {
    e = why;
    return false;
  }
}
bool activate(const QString &d, QString m, QString &e) {
  return transaction(d, e, [&](QJsonObject &s) {
    approved(s, m);
    s["activeModel"] = absolute(m);
    event(s, "activated", {{"modelPath", absolute(m)}});
  });
}
bool deactivate(const QString &d, QString reason, QString &e) {
  return transaction(d, e, [&](QJsonObject &s) {
    need(!reason.trimmed().isEmpty(), "Deactivation reason required");
    s["activeModel"] = QString();
    event(s, "deactivated", {{"reason", reason.trimmed()}});
  });
}
QString activeModel(const QString &d, QString &e) {
  e.clear();
  try {
    auto s = load(d);
    auto m = s["activeModel"].toString();
    if (m.isEmpty())
      return {};
    approved(s, m);
    return m;
  } catch (const QString &why) {
    e = why;
    return {};
  }
}
bool rollback(const QString &d, QString &e) {
  return transaction(d, e, [&](QJsonObject &s) {
    auto m = rollbackCandidate(s);
    s["activeModel"] = m;
    event(s, "rolled-back", {{"modelPath", m}});
  });
}
} // namespace LearningReview
