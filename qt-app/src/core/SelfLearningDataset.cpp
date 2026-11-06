#include "SelfLearningDataset.h"
#include "SchemeStore.h"
#include "ElectromagneticEngine.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>

namespace SelfLearning {
namespace {
const QString Scope = QStringLiteral("oil-foil-stacked-ellipse-tyoke-dual-cylinder-50hz-75c");
constexpr qsizetype MaxDatasetBytes = 16 * 1024 * 1024;
bool fail(QString &error, const QString &message) { error = message; return false; }
QString productGroup(const QString &id) { return id.trimmed().toCaseFolded(); }
bool validPurpose(const QString &purpose)
{
    return purpose == "unassigned" || purpose == "training"
        || purpose == "independent_test" || purpose == "excluded";
}
bool acquire(QLockFile &lock, const QString &path, QString &error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return fail(error, QStringLiteral("无法创建样本目录。"));
    if (!lock.tryLock(0)) return fail(error, QStringLiteral("另一个程序正在写入学习样本，请稍后重试；未覆盖任何数据。"));
    return true;
}

QJsonObject inputSchema(const QJsonValue &value)
{
    if (value.isObject()) {
        QJsonObject properties; QJsonArray required;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            properties[it.key()] = inputSchema(it.value()); required.append(it.key());
        }
        return {{"type", "object"}, {"properties", properties}, {"required", required}, {"additionalProperties", false}};
    }
    if (value.isArray()) {
        QJsonArray items;
        for (const auto &item : value.toArray()) items.append(inputSchema(item));
        return {{"type", "array"}, {"length", items.size()}, {"itemsByIndex", items}};
    }
    return {{"type", value.isDouble() ? "number" : value.isBool() ? "boolean" : "string"}};
}

bool finiteInput(const QJsonValue &v)
{
    if (v.isDouble()) return std::isfinite(v.toDouble()) && std::abs(v.toDouble()) <= 1000000.0;
    if (v.isArray()) { for (const auto &x : v.toArray()) if (!finiteInput(x)) return false; }
    else if (v.isObject()) { for (const auto &x : v.toObject()) if (!finiteInput(x)) return false; }
    else if (!v.isString() && !v.isBool()) return false;
    return true;
}

bool validLabels(const Sample &s, QString &error)
{
    if (s.id.trimmed().isEmpty() || s.productId.trimmed().isEmpty() || !s.testedOn.isValid())
        return fail(error, QStringLiteral("样本ID、产品/试验编号及有效试验日期不能为空。"));
    bool any = false;
    for (int i = 0; i < MetricCount; ++i) {
        if (!s.measured[i]) continue;
        any = true;
        const double v = *s.measured[i];
        if (!std::isfinite(v) || v < 0 || (i < 3 && v == 0))
            return fail(error, QStringLiteral("%1实测值无效；损耗/阻抗须大于0，温升允许0，未测请留空。")
                .arg(QString::fromUtf8(metrics()[i].name)));
    }
    if (any && s.source.trimmed().isEmpty())
        return fail(error, QStringLiteral("有实测值的样本必须填写试验报告等数据来源。"));
    return true;
}

bool validateList(const QVector<Sample> &samples, QString &error)
{
    QSet<QString> ids, tests;
    QHash<QString, QString> purposes;
    for (const auto &s : samples) {
        if (!validLabels(s, error)) return false;
        if (!validPurpose(s.purpose)) return fail(error, QStringLiteral("未知样本用途。"));
        const auto group = productGroup(s.productId);
        if (purposes.contains(group) && purposes.value(group) != s.purpose)
            return fail(error, QStringLiteral("同一产品的所有试验记录必须具有相同用途，请按产品整体分配。"));
        purposes.insert(group, s.purpose);
        const auto testKey = productGroup(s.productId) + QLatin1Char('\n') + s.testedOn.toString(Qt::ISODate);
        if (ids.contains(s.id.trimmed()) || tests.contains(testKey))
            return fail(error, QStringLiteral("存在重复样本ID或相同产品编号+试验日期；请合并实测项目后再导入。"));
        ids.insert(s.id.trimmed()); tests.insert(testKey);
        Sample checked;
        if (!decode(encode(s), checked, error)) return false;
    }
    error.clear(); return true;
}

bool parse(const QByteArray &bytes, QVector<Sample> &out, QString &error)
{
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return fail(error, QStringLiteral("样本文件不是有效JSON：%1").arg(parseError.errorString()));
    const auto root = doc.object();
    if (root.value("format") != QJsonValue("ZTBLD-LearningDataset")
        || root.value("version") != QJsonValue(1) || !root.value("samples").isArray())
        return fail(error, QStringLiteral("不支持的学习样本格式/版本。需要ZTBLD-LearningDataset v1。"));
    QVector<Sample> staged;
    QHash<QString, QJsonObject> seen;
    for (const auto &v : root.value("samples").toArray()) {
        Sample s;
        if (!v.isObject() || !decode(v.toObject(), s, error))
            return fail(error, QStringLiteral("第%1个样本：%2").arg(staged.size() + 1).arg(error));
        const auto encoded = encode(s);
        if (seen.contains(s.id)) {
            if (seen.value(s.id) != encoded) return fail(error, QStringLiteral("同一导入文件内存在冲突样本ID：%1").arg(s.id));
            continue;
        }
        seen.insert(s.id, encoded); staged.append(s);
    }
    if (!validateList(staged, error)) return false;
    out = staged; error.clear(); return true;
}
}

const std::array<Metric, MetricCount> &metrics()
{
    static const std::array<Metric, MetricCount> list{{
        {"noLoadLoss_W", "空载损耗", "W"}, {"loadLoss_W", "负载损耗", "W"},
        {"impedance_pct", "阻抗电压", "%"}, {"oilTopRise_K", "油顶层温升", "K"},
        {"hvWindingRise_K", "高压绕组温升", "K"}, {"lvWindingRise_K", "低压绕组温升", "K"}
    }};
    return list;
}

// Bump whenever the underlying formula/data tables or input schema change.
QString engineVersion() { return QStringLiteral("sb20-default-sheet12-learning-v1"); }
QString datasetPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/self_learning/samples.json");
}

bool makeSample(const CalcInput &input, Sample &out, QString &error)
{
    if (!finiteInput(SchemeStore::toJson(input)))
        return fail(error, QStringLiteral("设计参数含空值、非有限值或超出框架导入范围。"));
    if (!input.coreSelectionError().isEmpty() || input.hvCoilFormIdx != 1)
        return fail(error, QStringLiteral("本批样本仅支持完整椭圆/T形轭、多层圆筒式计算，不接受局部预览。"));
    CalcResult r;
    ElectromagneticEngine engine;
    if (!engine.calcElectromagnetic(input, r) || !r.valid)
        return fail(error, QStringLiteral("该输入无法完成原公式整机计算，不能建立学习样本。"));
    Sample s;
    s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.testedOn = QDate::currentDate(); s.input = input;
    s.baseline = {r.core.noLoadLoss_W, r.winding.loadLoss_W, r.impedance.impedance_pct,
                  r.thermal.oilTopRise_K, r.thermal.hvWindingRise_K, r.thermal.lvWindingRise_K};
    for (double v : s.baseline)
        if (!std::isfinite(v) || v <= 0)
            return fail(error, QStringLiteral("原公式输出存在无效指标，不能用于残差学习。"));
    out = s; error.clear(); return true;
}

QJsonObject encode(const Sample &s)
{
    QJsonObject base, measured;
    for (int i = 0; i < MetricCount; ++i) {
        base[metrics()[i].key] = s.baseline[i];
        measured[metrics()[i].key] = s.measured[i] ? QJsonValue(*s.measured[i]) : QJsonValue(QJsonValue::Null);
    }
    return {{"id", s.id}, {"productId", s.productId.trimmed()}, {"schemeName", s.schemeName},
        {"source", s.source.trimmed()}, {"testedOn", s.testedOn.toString(Qt::ISODate)},
        {"scope", Scope}, {"engineVersion", engineVersion()}, {"input", SchemeStore::toJson(s.input)},
        {"baseline", base}, {"measured", measured}, {"purpose", s.purpose}};
}

bool decode(const QJsonObject &o, Sample &out, QString &error)
{
    const auto purpose = o.value("purpose");
    if (!purpose.isUndefined() && (!purpose.isString() || !validPurpose(purpose.toString())))
        return fail(error, QStringLiteral("purpose必须为unassigned、training、independent_test或excluded。"));
    if (o.value("scope") != QJsonValue(Scope) || o.value("engineVersion") != QJsonValue(engineVersion()))
        return fail(error, QStringLiteral("样本适用结构或公式版本不匹配，禁止静默迁移。"));
    for (const auto key : {"id", "productId", "schemeName", "source", "testedOn"})
        if (!o.value(key).isString()) return fail(error, QStringLiteral("字段%1必须为文本。").arg(key));
    if (!o.value("input").isObject() || !o.value("baseline").isObject() || !o.value("measured").isObject())
        return fail(error, QStringLiteral("input/baseline/measured必须为完整对象。"));
    const auto inputJson = o.value("input").toObject();
    const auto input = SchemeStore::fromJson(inputJson);
    if (!finiteInput(inputJson) || SchemeStore::toJson(input) != inputJson)
        return fail(error, QStringLiteral("设计快照缺字段、类型/范围错误或含未知字段；请从程序重新导出模板。"));
    Sample s;
    if (!makeSample(input, s, error)) return false;
    s.purpose = purpose.isUndefined() ? QStringLiteral("unassigned") : purpose.toString();
    s.id = o.value("id").toString().trimmed(); s.productId = o.value("productId").toString().trimmed();
    s.schemeName = o.value("schemeName").toString(); s.source = o.value("source").toString().trimmed();
    s.testedOn = QDate::fromString(o.value("testedOn").toString(), Qt::ISODate);
    const auto base = o.value("baseline").toObject(), labels = o.value("measured").toObject();
    if (base.size() != MetricCount || labels.size() != MetricCount)
        return fail(error, QStringLiteral("原值和实测对象须包含全部六项指标，未测项填null。"));
    for (int i = 0; i < MetricCount; ++i) {
        const auto b = base.value(metrics()[i].key), v = labels.value(metrics()[i].key);
        if (!b.isDouble() || !std::isfinite(b.toDouble()) || std::abs(b.toDouble() - s.baseline[i]) > 1e-7)
            return fail(error, QStringLiteral("原公式结果与设计快照重算不符，禁止混用修正值或其他版本结果。"));
        if (v.isDouble()) s.measured[i] = v.toDouble();
        else if (!v.isNull()) return fail(error, QStringLiteral("实测值须为数值或null，不接受数字文本。"));
    }
    if (!validLabels(s, error)) return false;
    out = s; error.clear(); return true;
}

QJsonObject document(const QVector<Sample> &samples)
{
    QJsonArray rows;
    for (const auto &s : samples) rows.append(encode(s));
    return {{"format", "ZTBLD-LearningDataset"}, {"version", 1}, {"samples", rows}};
}

bool load(const QString &path, QVector<Sample> &out, QString &error)
{
    if (!QFileInfo::exists(path)) { out.clear(); error.clear(); return true; }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return fail(error, f.errorString());
    if (f.size() > MaxDatasetBytes) return fail(error, QStringLiteral("样本文件超过16MB，请分批处理。"));
    return parse(f.readAll(), out, error);
}

bool writeJson(const QString &path, const QJsonObject &json, QString &error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return fail(error, QStringLiteral("无法创建本地目录。"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    const auto bytes = QJsonDocument(json).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) return fail(error, file.errorString());
    error.clear(); return true;
}

namespace {
bool saveUnlocked(const QString &path, const QVector<Sample> &samples, QString &error)
{
    if (!validateList(samples, error)) return false;
    const auto json = document(samples);
    if (QJsonDocument(json).toJson(QJsonDocument::Indented).size() > MaxDatasetBytes)
        return fail(error, QStringLiteral("保存后的样本库超过16MB，已保留原文件；请使用另一个样本库分批管理。"));
    return writeJson(path, json, error);
}
}

bool save(const QString &path, const QVector<Sample> &samples, QString &error)
{
    QLockFile lock(path + ".lock");
    if (!acquire(lock, path, error)) return false;
    return saveUnlocked(path, samples, error);
}

bool importDataset(const QString &path, const QByteArray &bytes, QString &error)
{
    QVector<Sample> incoming, merged;
    if (bytes.size() > MaxDatasetBytes) return fail(error, QStringLiteral("导入文件超过16MB。"));
    if (!parse(bytes, incoming, error)) return false;
    QLockFile lock(path + ".lock");
    if (!acquire(lock, path, error) || !load(path, merged, error)) return false;
    for (const auto &s : incoming) {
        bool found = false;
        for (const auto &old : merged) {
            if (old.id != s.id) continue;
            if (encode(old) != encode(s)) return fail(error, QStringLiteral("样本ID冲突：%1。请在界面打开原样本修改，不自动覆盖。").arg(s.id));
            found = true; break;
        }
        if (!found) merged.append(s);
    }
    return saveUnlocked(path, merged, error);
}

bool updateSample(const QString &path, const Sample &sample, const QJsonObject &original, QString &error)
{
    QLockFile lock(path + ".lock");
    if (!acquire(lock, path, error)) return false;
    QVector<Sample> current;
    if (!load(path, current, error)) return false;
    bool found = false;
    for (auto &s : current) if (s.id == sample.id) {
        if (original.isEmpty() || encode(s) != original)
            return fail(error, QStringLiteral("该样本已被其他窗口修改，请重新读取后编辑。"));
        s = sample; found = true; break;
    }
    if (!found && !original.isEmpty()) return fail(error, QStringLiteral("原样本已不在库中，请重新读取。"));
    if (!found) current.append(sample);
    return saveUnlocked(path, current, error);
}

QString purposeName(QString purpose)
{
    if (purpose == "unassigned") return QStringLiteral("待分区");
    if (purpose == "training") return QStringLiteral("训练用");
    if (purpose == "independent_test") return QStringLiteral("独立测试保留");
    if (purpose == "excluded") return QStringLiteral("暂不使用");
    return QStringLiteral("未知用途：%1").arg(purpose);
}
bool validateDataset(const QVector<Sample> &samples, QString &error)
{ return validateList(samples, error); }

bool assignProductPurpose(const QString &path, const QString &productId, const QString &purpose,
                          const QVector<Sample> &expectedGroup, QString &error)
{
    const auto group = productGroup(productId);
    if (group.isEmpty() || !validPurpose(purpose) || expectedGroup.isEmpty())
        return fail(error, QStringLiteral("产品、用途或预期产品快照无效。"));
    if (!validateList(expectedGroup, error)) return false;
    QHash<QString, QJsonObject> expected;
    for (const auto &s : expectedGroup) {
        if (productGroup(s.productId) != group)
            return fail(error, QStringLiteral("预期快照包含其他产品。"));
        expected.insert(s.id, encode(s));
    }
    QLockFile lock(path + ".lock");
    if (!acquire(lock, path, error)) return false;
    QVector<Sample> current;
    if (!load(path, current, error)) return false;
    int matched = 0;
    for (auto &s : current) {
        if (productGroup(s.productId) != group) continue;
        if (!expected.contains(s.id) || expected.value(s.id) != encode(s))
            return fail(error, QStringLiteral("产品记录已变化，请重新读取后分配用途。"));
        ++matched;
        s.purpose = purpose;
    }
    if (matched != expected.size())
        return fail(error, QStringLiteral("产品记录已新增或删除，请重新读取后分配用途。"));
    return saveUnlocked(path, current, error);
}

bool updateBatch(const QString &path, const QVector<Sample> &replacements,
                 const QVector<Sample> &originals, QString &error)
{
    if (replacements.isEmpty() || replacements.size() != originals.size())
        return fail(error, QStringLiteral("批量更新需要数量相同且非空的新旧快照。"));
    if (!validateList(replacements, error) || !validateList(originals, error)) return false;
    QHash<QString, QJsonObject> snapshots;
    QHash<QString, Sample> edits;
    for (const auto &s : originals) snapshots.insert(s.id, encode(s));
    for (const auto &s : replacements) {
        if (!snapshots.contains(s.id))
            return fail(error, QStringLiteral("批量更新的新旧样本ID必须完全一致。"));
        edits.insert(s.id, s);
    }
    QLockFile lock(path + ".lock");
    if (!acquire(lock, path, error)) return false;
    QVector<Sample> current;
    if (!load(path, current, error)) return false;
    int matched = 0;
    for (auto &s : current) {
        if (!edits.contains(s.id)) continue;
        if (encode(s) != snapshots.value(s.id))
            return fail(error, QStringLiteral("批量更新存在过期快照，请重新读取；未保存任何修改。"));
        s = edits.value(s.id);
        ++matched;
    }
    if (matched != edits.size())
        return fail(error, QStringLiteral("批量更新的原样本已不存在，不允许追加。"));
    return saveUnlocked(path, current, error);
}

std::array<int, MetricCount> labelledCounts(const QVector<Sample> &samples)
{
    std::array<int, MetricCount> counts{};
    for (const auto &s : samples) for (int i = 0; i < MetricCount; ++i) if (s.measured[i]) ++counts[i];
    return counts;
}

namespace {
bool purposeBundle(const QVector<Sample> &samples, const QString &purpose, QJsonObject &out, QString &error)
{
    if (!validateList(samples, error)) return false;
    QJsonArray rows;
    for (const auto &s : samples) {
        if (purpose == "independent_test" && s.purpose != purpose)
            return fail(error, QStringLiteral("评估包只能选择独立测试用途的样本。"));
        if (s.purpose != purpose) continue;
        QJsonObject residual;
        bool any = false;
        for (int i = 0; i < MetricCount; ++i) {
            any |= s.measured[i].has_value();
            residual[metrics()[i].key] = s.measured[i] ? QJsonValue(*s.measured[i] - s.baseline[i]) : QJsonValue(QJsonValue::Null);
        }
        if (!any) continue;
        auto row = encode(s); row["residual"] = residual; row["productGroupId"] = productGroup(s.productId); rows.append(row);
    }
    if (rows.isEmpty()) return fail(error, QStringLiteral("所选用途中尚无实测标签，不能导出交接包。"));
    QJsonArray targets;
    for (const auto &m : metrics()) targets.append(QJsonObject{{"key", m.key}, {"unit", m.unit}});
    const auto hash = QCryptographicHash::hash(QJsonDocument(rows).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex();
    auto schema = inputSchema(SchemeStore::toJson(CalcInput{}));
    CalcInput withCriteria;
    withCriteria.hasPerformanceCriteria = true;
    auto properties = schema.value("properties").toObject();
    auto criteriaSchema = inputSchema(SchemeStore::toJson(withCriteria).value("performanceCriteria"));
    auto criteriaProperties = criteriaSchema.value("properties").toObject();
    criteriaProperties["nonstandardAn3_pct"] = QJsonObject{{"type", "number"}};
    criteriaProperties["savedCustomNoLoadCurrent_pct"] = QJsonObject{{"type", "number"}};
    criteriaProperties["noLoadCurrentSource"]=QJsonObject{{"type","string"},{"enum",QJsonArray{"automatic","manual"}}};
    QJsonArray legacyRequired;for(const auto &key:criteriaSchema.value("required").toArray())if(key.toString()!="noLoadCurrentSource")legacyRequired.append(key);
    criteriaSchema["required"]=legacyRequired; // 旧数据没有此字段，按方案迁移规则恢复。
    criteriaSchema["properties"] = criteriaProperties;
    properties["performanceCriteria"] = criteriaSchema;
    schema["properties"] = properties; // Optional: intentionally absent from the root required list.
    schema["id"] = "ztbld-calcinput-learning-v1";
    schema["numericValidation"] = "finite, abs<=1000000; exact CalcInput serialization and successful full-engine calculation required";
    out = {{"format", "ZTBLD-ResidualTrainingBundle"}, {"version", 1}, {"engineVersion", engineVersion()},
        {"scope", Scope}, {"bundleUse", purpose}, {"targets", targets}, {"datasetSha256", QString::fromLatin1(hash)},
        {"inputSchema", schema}, {"splitGroupKey", "productGroupId"}, {"residualDefinition", "measured-baseline"}, {"rows", rows}};
    error.clear(); return true;
}
}

bool trainingBundle(const QVector<Sample> &samples, QJsonObject &out, QString &error)
{ return purposeBundle(samples, QStringLiteral("training"), out, error); }

bool evaluationBundle(const QVector<Sample> &samples, QJsonObject &out, QString &error)
{ return purposeBundle(samples, QStringLiteral("independent_test"), out, error); }

bool freezeDatasetBundle(const QString &path, const QVector<Sample> &expectedDataset,
                         const QStringList &selectedIds, bool evaluation, QJsonObject &out, QString &error)
{
    out = QJsonObject{};
    QLockFile lock(path + ".lock");
    if (!acquire(lock, path, error)) return false;
    // load() intentionally treats missing files as empty for editors. Freezing
    // requires a real persisted source and must never use the caller's cache.
    if (path.trimmed().isEmpty() || !QFileInfo::exists(path))
        return fail(error, QStringLiteral("样本库文件不存在，请重新读取后再创建交接包。"));
    QVector<Sample> current;
    if (!load(path, current, error) || !validateList(current, error)
        || !validateList(expectedDataset, error)) return false;
    if (current.size() != expectedDataset.size())
        return fail(error, QStringLiteral("样本库记录已新增或删除，请重新读取后再创建交接包。"));
    QHash<QString, QJsonObject> expected;
    for (const auto &s : expectedDataset) expected.insert(s.id, encode(s));
    for (const auto &s : current) {
        if (!expected.contains(s.id) || expected.value(s.id) != encode(s))
            return fail(error, QStringLiteral("样本库内容或用途已变化，请重新读取后再创建交接包。"));
    }
    QJsonObject frozen;
    if (!evaluation) {
        if (!selectedIds.isEmpty())
            return fail(error, QStringLiteral("训练冻结必须使用完整样本库，不接受样本选择列表。"));
        if (!trainingBundle(current, frozen, error)) return false;
    } else {
        if (selectedIds.isEmpty())
            return fail(error, QStringLiteral("请选择独立测试样本后再冻结评估包。"));
        QSet<QString> selected;
        for (const auto &id : selectedIds) {
            if (selected.contains(id))
                return fail(error, QStringLiteral("独立测试样本选择包含重复ID。"));
            selected.insert(id);
        }
        QVector<Sample> subset;
        for (const auto &s : current) if (selected.contains(s.id)) subset.append(s);
        if (subset.size() != selected.size())
            return fail(error, QStringLiteral("所选独立测试样本已不存在，请重新读取。"));
        if (!evaluationBundle(subset, frozen, error)) return false;
    }
    // Publish only the fully constructed logical snapshot, while still locked.
    out = frozen;
    error.clear(); return true;
}

QString trainingBlockReason(const QVector<Sample> &samples)
{
    const auto counts = labelledCounts(samples);
    bool any = false; for (int n : counts) any |= n > 0;
    return any ? QStringLiteral("已有实测标签；本批尚未接入DNN训练后端，可先导出训练交接包。")
               : QStringLiteral("数据不足：尚无实测标签。先保存样本并补录真实试验数据。" );
}
QString correctionUnavailableReason()
{
    return QStringLiteral("尚无经验证模型，推理运行时未接入；DNN修正值不可用。原公式计算保持不变。" );
}
}
