#include "LearningCandidateComparison.h"
#include "SelfLearningDataset.h"
#include "SchemeStore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>
#include <cmath>
#include <climits>

namespace LearningComparison {
namespace {
constexpr qsizetype MaxBytes = 16 * 1024 * 1024;
void need(bool condition, const QString &why) { if (!condition) throw why; }
bool number(const QJsonValue &v, bool positive = false) {
    return v.isDouble() && std::isfinite(v.toDouble()) && (positive ? v.toDouble() > 0 : v.toDouble() >= 0);
}
bool integer(const QJsonValue &v) {
    return number(v, true) && v.toDouble() <= INT_MAX && std::floor(v.toDouble()) == v.toDouble();
}
bool finiteInput(const QJsonValue &v) {
    if (v.isDouble()) return std::isfinite(v.toDouble()) && std::abs(v.toDouble()) <= 1000000.0;
    if (v.isObject()) { for (const auto &x : v.toObject()) if (!finiteInput(x)) return false; return true; }
    if (v.isArray()) { for (const auto &x : v.toArray()) if (!finiteInput(x)) return false; return true; }
    return v.isString() || v.isBool();
}
void checkCalcInput(const QJsonValue &v) {
    need(v.isObject() && finiteInput(v), "Invalid serialized CalcInput: null, nonfinite or out of bounds");
    const auto o = v.toObject();
    need(SchemeStore::toJson(SchemeStore::fromJson(o)) == o, "CalcInput shape/types/fields do not round-trip exactly");
}
void checkMetrics(const QJsonValue &v) {
    need(v.isObject() && v.toObject().size() == SelfLearning::MetricCount, "Expected exactly six baseline metrics");
    const auto o = v.toObject();
    for (int i=0; i<SelfLearning::MetricCount; ++i)
        need(number(o[SelfLearning::metrics()[i].key], i<3), "Invalid baseline metric");
}
QJsonObject sampleTemplate() { return SelfLearning::encode(SelfLearning::Sample{}); }
void checkInput(const QJsonObject &input) {
    need(input.keys()==QStringList{"candidates","format","runId","usage","version"}, "Unknown comparison input fields");
    need(input["format"] == QJsonValue("ZTBLD-CandidateComparisonInput") && input["version"] == QJsonValue(1)
         && input["usage"] == QJsonValue("independent-comparison-only")
         && input["runId"].isString() && !QUuid(input["runId"].toString()).isNull()
         && QUuid(input["runId"].toString()).toString(QUuid::WithoutBraces)==input["runId"].toString(), "Invalid comparison input metadata");
    need(QJsonDocument(input).toJson().size() <= MaxBytes, "Comparison input exceeds 16 MiB; no candidates were clipped");
    need(input["candidates"].isArray() && !input["candidates"].toArray().isEmpty(), "Missing candidate snapshot");
    const auto prototype=sampleTemplate();
    QSet<int> ids;
    int order=0;
    for (const auto &v : input["candidates"].toArray()) {
        need(v.isObject(), "Invalid candidate row");
        const auto c=v.toObject();
        need(c.keys()==QStringList{"candidateId","costBasis","materialCost","originalOrder","sample","schemeId"}, "Unknown candidate input fields");
        need(integer(c["schemeId"]) && !ids.contains(c["schemeId"].toInt()), "Candidate scheme IDs must be positive and unique");
        ids.insert(c["schemeId"].toInt());
        need(c["originalOrder"] == QJsonValue(++order) && c["candidateId"] ==
             QJsonValue(QStringLiteral("scheme-%1").arg(c["schemeId"].toInt())), "Candidate identity/order mismatch");
        need(number(c["materialCost"]) && c["costBasis"].isString(), "Invalid candidate cost or cost basis");
        need(c["sample"].isObject(), "Missing candidate sample");
        const auto s=c["sample"].toObject();
        need(s.keys()==prototype.keys() && s["id"]==c["candidateId"] && s["productId"]==c["candidateId"]
             && s["schemeName"].isString() && s["source"]==QJsonValue("optimization-candidate")
             && s["purpose"]==QJsonValue("unassigned") && s["scope"]==prototype["scope"]
             && s["engineVersion"]==prototype["engineVersion"] && s["testedOn"].isString()
             && QDate::fromString(s["testedOn"].toString(),Qt::ISODate).isValid(), "Invalid independent sample metadata");
        checkCalcInput(s["input"]); checkMetrics(s["baseline"]);
        need(s["measured"].isObject() && s["measured"].toObject()==prototype["measured"].toObject(), "Comparison samples must have six null measured labels");
    }
}
}

QJsonObject makeInput(const QVector<OptimizeCandidate> &candidates, QString &error) {
    error.clear();
    try {
        QJsonArray rows;
        QSet<int> ids;
        qsizetype rowBytes=0;
        for (const auto &candidate : candidates) {
            const int id=candidate.scheme.schemeIdx;
            need(id>0 && !ids.contains(id), "Candidate scheme IDs must be positive and unique");
            ids.insert(id);
            need(candidate.result.valid, "Original candidate result is invalid");
            checkCalcInput(SchemeStore::toJson(candidate.input));
            SelfLearning::Sample s;
            s.id=QStringLiteral("scheme-%1").arg(id); s.productId=s.id;
            s.source=QStringLiteral("optimization-candidate"); s.testedOn=QDate::currentDate();
            s.input=candidate.input;
            const auto &r=candidate.result;
            s.baseline={r.core.noLoadLoss_W,r.winding.loadLoss_W,r.impedance.impedance_pct,
                        r.thermal.oilTopRise_K,r.thermal.hvWindingRise_K,r.thermal.lvWindingRise_K};
            for (int i=0;i<SelfLearning::MetricCount;++i)
                need(std::isfinite(s.baseline[i]) && (i<3 ? s.baseline[i]>0 : s.baseline[i]>=0), "Invalid original baseline metric");
            need(std::isfinite(r.cost.materialCost) && r.cost.materialCost>=0, "Invalid original material cost");
            QJsonObject row{{"candidateId",s.id},{"schemeId",id},{"originalOrder",rows.size()+1},
                {"materialCost",r.cost.materialCost},{"costBasis",candidate.scheme.costBasis},{"sample",SelfLearning::encode(s)}};
            rowBytes+=QJsonDocument(row).toJson().size();
            need(rowBytes<=MaxBytes, "Comparison input exceeds 16 MiB; no candidates were clipped");
            rows.append(row);
        }
        QJsonObject out{{"format","ZTBLD-CandidateComparisonInput"},{"version",1},
            {"usage","independent-comparison-only"},{"runId",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"candidates",rows}};
        checkInput(out);
        return out;
    } catch (const QString &why) { error=why; return {}; }
}

namespace {
QString normalizedPath(const QString &path) {
    need(!path.trimmed().isEmpty(), "Missing artifact path");
    QFileInfo info(QDir::fromNativeSeparators(path));
    QString normalized=info.canonicalFilePath();
    if (normalized.isEmpty()) normalized=info.absoluteFilePath();
    normalized=QDir::cleanPath(normalized);
#ifdef Q_OS_WIN
    normalized=normalized.toCaseFolded();
#endif
    return normalized;
}
QString digest(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
QByteArray readBytes(const QString &path) {
    QFile file(path);
    need(file.open(QIODevice::ReadOnly), "Cannot read artifact: "+path);
    need(file.size()>0 && file.size()<=MaxBytes, "Artifact is empty or exceeds 16 MiB: "+path);
    const auto bytes=file.read(MaxBytes+1);
    need(file.error()==QFileDevice::NoError && !bytes.isEmpty() && bytes.size()<=MaxBytes && file.atEnd(), "Cannot read bounded artifact: "+path);
    return bytes;
}
QJsonObject parse(const QByteArray &bytes) {
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(bytes,&error);
    need(error.error==QJsonParseError::NoError && doc.isObject(), "Artifact must contain a JSON object");
    return doc.object();
}
void safeFlags(const QJsonObject &o) {
    need(o["testOnly"].isBool() && o["productionEligible"]==QJsonValue(false), "Invalid model/report eligibility flags");
    if (o.contains("synthetic"))
        need(o["synthetic"].isBool() && (!o["synthetic"].toBool() || o["testOnly"]==QJsonValue(true)), "Synthetic artifact cannot be elevated");
}
}

bool validateReport(const QJsonObject &input, const QJsonObject &report,
                    const QString &modelPath, const QString &inputPath, QString &error) {
    error.clear();
    try {
        checkInput(input);
        need(report.keys()==QStringList{"candidates","counts","engineVersion","format","inputPath","inputSha256",
             "modelPath","modelSha256","modelVersion","productionEligible","reportSha256","runId","scope","status","testOnly","usage","version"},
             "Unknown or missing comparison report fields");
        need(QJsonDocument(report).toJson().size()<=MaxBytes, "Comparison report exceeds 16 MiB");
        need(report["format"]==QJsonValue("ZTBLD-CandidateComparison") && report["version"]==QJsonValue(1)
             && report["status"]==QJsonValue("completed") && report["usage"]==input["usage"]
             && report["runId"]==input["runId"], "Comparison report metadata/run mismatch");
        need(normalizedPath(report["inputPath"].toString())==normalizedPath(inputPath)
             && normalizedPath(report["modelPath"].toString())==normalizedPath(modelPath), "Comparison artifact path mismatch");
        const auto inputBytes=readBytes(inputPath);
        need(report["inputSha256"]==QJsonValue(digest(inputBytes)) && parse(inputBytes)==input,
             "Input file digest/content does not match immutable snapshot");
        const auto modelBytes=readBytes(modelPath);
        const auto siblingPath=QFileInfo(QFileInfo(modelPath).canonicalFilePath()).dir().filePath("report.json");
        const auto siblingBytes=readBytes(siblingPath);
        need(report["modelSha256"]==QJsonValue(digest(modelBytes))
             && report["reportSha256"]==QJsonValue(digest(siblingBytes)), "Current model/report digest mismatch");
        const auto model=parse(modelBytes), sibling=parse(siblingBytes);
        safeFlags(model); safeFlags(sibling); safeFlags(report);
        need(model["format"]==QJsonValue("ZTBLD-ResidualModel") && model["version"]==QJsonValue(1)
             && model["status"]==QJsonValue("experimental") && model["modelVersion"].isString()
             && !model["modelVersion"].toString().trimmed().isEmpty(), "Invalid model metadata");
        const auto prototype=sampleTemplate();
        need(model["scope"]==prototype["scope"] && model["engineVersion"]==prototype["engineVersion"], "Unknown model scope or engine version");
        need(sibling["status"]==QJsonValue("completed") && sibling["modelVersion"]==model["modelVersion"]
             && sibling["testOnly"]==model["testOnly"]
             && normalizedPath(sibling["modelPath"].toString())==normalizedPath(modelPath), "Sibling model report incomplete or inconsistent");
        for (const auto *key : {"scope","engineVersion"})
            if (sibling.contains(key)) need(sibling[key]==model[key], "Sibling model report scope/version mismatch");
        for (const auto *key : {"modelVersion","scope","engineVersion","testOnly","productionEligible"})
            need(report[key]==model[key], QStringLiteral("Comparison model metadata mismatch: %1").arg(key));
        const auto originals=input["candidates"].toArray();
        need(report["candidates"].isArray() && report["candidates"].toArray().size()==originals.size(), "Comparison candidate count mismatch");
        const auto rows=report["candidates"].toArray();
        int successes=0, failures=0;
        for (qsizetype index=0;index<rows.size();++index) {
            need(rows[index].isObject(), "Invalid comparison result row");
            const auto row=rows[index].toObject(), original=originals[index].toObject();
            need(row.keys()==QStringList{"baseline","candidateId","costBasis","failures","materialCost","originalOrder","predictions","schemeId"}, "Unknown or missing candidate result fields");
            for (const auto *key : {"candidateId","schemeId","originalOrder","materialCost","costBasis"})
                need(row[key]==original[key], QStringLiteral("Original candidate binding mismatch: %1").arg(key));
            need(row["baseline"]==original["sample"].toObject()["baseline"], "Original baseline changed");
            need(row["predictions"].isObject() && row["predictions"].toObject().size()==SelfLearning::MetricCount
                 && row["failures"].isObject(), "Predictions must contain exactly six metrics with failure coverage");
            const auto predictions=row["predictions"].toObject(), reasons=row["failures"].toObject();
            int rowFailures=0;
            for (int i=0;i<SelfLearning::MetricCount;++i) {
                const QString key=QString::fromLatin1(SelfLearning::metrics()[i].key);
                const auto value=predictions[key];
                if (value.isNull()) {
                    need(reasons[key].isString() && !reasons[key].toString().trimmed().isEmpty(), "Null prediction must have a readable failure reason");
                    ++rowFailures; ++failures;
                } else {
                    need(number(value,i<3) && !reasons.contains(key), "Invalid prediction or failure attached to successful target");
                    ++successes;
                }
            }
            need(reasons.size()==rowFailures, "Failure keys must match exactly the failed prediction keys");
        }
        need(report["counts"].isObject() && report["counts"].toObject()==QJsonObject{{"candidates",rows.size()},
             {"successfulTargets",successes},{"failedTargets",failures}}, "Comparison summary counts mismatch");
        // Detect evidence replacement during validation as well as before entry.
        need(readBytes(inputPath)==inputBytes && readBytes(modelPath)==modelBytes && readBytes(siblingPath)==siblingBytes,
             "Comparison evidence changed during validation");
        return true;
    } catch (const QString &why) { error=why; return false; }
}
}
