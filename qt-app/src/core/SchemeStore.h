#ifndef SCHEMESTORE_H
#define SCHEMESTORE_H
// 方案库持久化：批量方案的完整设计变量（CalcInput）↔ JSON 文件。
// 只存输入不存结果——加载后由引擎重算得到结果（计算确定性，结果一致），
// 文件体积小且天然兼容引擎后续修正

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include "CalcInput.h"

namespace SchemeStore {

// ---- CalcInput ↔ JSON ----
inline QJsonObject toJson(const CalcInput &in)
{
    QJsonObject o;
    // 额定值
    o.insert(QStringLiteral("capacity_kVA"), in.capacity_kVA);
    o.insert(QStringLiteral("hvRated_kV"), in.hvRated_kV);
    o.insert(QStringLiteral("lvRated_kV"), in.lvRated_kV);
    o.insert(QStringLiteral("hvTapMax_pct"), in.hvTapMax_pct);
    o.insert(QStringLiteral("hvTapMin_pct"), in.hvTapMin_pct);
    o.insert(QStringLiteral("hvDeltaConnected"), in.hvDeltaConnected);
    o.insert(QStringLiteral("lvStarConnected"), in.lvStarConnected);
    // 铁芯
    o.insert(QStringLiteral("coreDiameter_mm"), in.coreDiameter_mm);
    o.insert(QStringLiteral("coreStraight_mm"), in.coreStraight_mm);
    o.insert(QStringLiteral("ellipseAngle_deg"), in.ellipseAngle_deg);
    o.insert(QStringLiteral("stackFactor"), in.stackFactor);
    o.insert(QStringLiteral("steelThickness_mm"), in.steelThickness_mm);
    o.insert(QStringLiteral("steelGrade"), in.steelGrade);
    o.insert(QStringLiteral("seamCount"), in.seamCount);
    o.insert(QStringLiteral("coreLossCraftCoef"), in.coreLossCraftCoef);
    o.insert(QStringLiteral("yokePiece1Stack_mm"), in.yokePiece1Stack_mm);
    o.insert(QStringLiteral("yokePiece2Stack_mm"), in.yokePiece2Stack_mm);
    o.insert(QStringLiteral("yokePiece3Stack_mm"), in.yokePiece3Stack_mm);
    o.insert(QStringLiteral("yokeWidenTo_mm"), in.yokeWidenTo_mm);
    o.insert(QStringLiteral("yokeWidenStages"), in.yokeWidenStages);
    QJsonArray steps;
    for (double v : in.yokeSteps_mm) {
        steps.append(v);
    }
    o.insert(QStringLiteral("yokeSteps_mm"), steps);
    // 高压绕组
    o.insert(QStringLiteral("hvBareWidth_mm"), in.hvBareWidth_mm);
    o.insert(QStringLiteral("hvBareThick_mm"), in.hvBareThick_mm);
    o.insert(QStringLiteral("hvParallelCount"), in.hvParallelCount);
    o.insert(QStringLiteral("hvStackCount"), in.hvStackCount);
    o.insert(QStringLiteral("hvTurnsPerLayer"), in.hvTurnsPerLayer);
    o.insert(QStringLiteral("hvLayerInsul_mm"), in.hvLayerInsul_mm);
    o.insert(QStringLiteral("hvWireInsulAdd_mm"), in.hvWireInsulAdd_mm);
    o.insert(QStringLiteral("hvCoilFormIdx"), in.hvCoilFormIdx);
    QJsonArray dws, dhs;
    for (double v : in.hvDuctWidthSide) {
        dws.append(v);
    }
    for (double v : in.hvDuctHeightSide) {
        dhs.append(v);
    }
    o.insert(QStringLiteral("hvDuctWidthSide"), dws);
    o.insert(QStringLiteral("hvDuctHeightSide"), dhs);
    // 低压绕组
    o.insert(QStringLiteral("lvTurns"), in.lvTurns);
    o.insert(QStringLiteral("lvFoilThick_mm"), in.lvFoilThick_mm);
    o.insert(QStringLiteral("lvFoilWidth_mm"), in.lvFoilWidth_mm);
    o.insert(QStringLiteral("lvLayerInsulCount"), in.lvLayerInsulCount);
    o.insert(QStringLiteral("lvLayerInsul_mm"), in.lvLayerInsul_mm);
    o.insert(QStringLiteral("lvEndInsul_mm"), in.lvEndInsul_mm);
    o.insert(QStringLiteral("lvCopperFoil"), in.lvCopperFoil);
    QJsonArray lws, lhs;
    for (double v : in.lvDuctWidthSide) {
        lws.append(v);
    }
    for (double v : in.lvDuctHeightSide) {
        lhs.append(v);
    }
    o.insert(QStringLiteral("lvDuctWidthSide"), lws);
    o.insert(QStringLiteral("lvDuctHeightSide"), lhs);
    // 主空道与损耗系数
    o.insert(QStringLiteral("mainDuctWidth_mm"), in.mainDuctWidth_mm);
    o.insert(QStringLiteral("mainDuctInsul_mm"), in.mainDuctInsul_mm);
    o.insert(QStringLiteral("strayLossFactor"), in.strayLossFactor);
    o.insert(QStringLiteral("leadLoss_W"), in.leadLoss_W);
    o.insert(QStringLiteral("lvExtraLoss_W"), in.lvExtraLoss_W);
    // 油箱与结构常数
    o.insert(QStringLiteral("tankBottomOil_mm"), in.tankBottomOil_mm);
    o.insert(QStringLiteral("tankSideClear_mm"), in.tankSideClear_mm);
    o.insert(QStringLiteral("tankEndClear_mm"), in.tankEndClear_mm);
    o.insert(QStringLiteral("tankFoot_mm"), in.tankFoot_mm);
    o.insert(QStringLiteral("waveDepth_mm"), in.waveDepth_mm);
    o.insert(QStringLiteral("waveHeight_mm"), in.waveHeight_mm);
    o.insert(QStringLiteral("wavePitch_mm"), in.wavePitch_mm);
    o.insert(QStringLiteral("phaseGapBase_mm"), in.phaseGapBase_mm);
    o.insert(QStringLiteral("refFluxDens_T"), in.refFluxDens_T);
    return o;
}

// 缺失字段用 CalcInput 默认值（结构体默认初始化）兜底
inline CalcInput fromJson(const QJsonObject &o)
{
    CalcInput in;
    const auto num = [&o](const char *key, double dst) {
        const auto v = o.value(QLatin1String(key));
        return v.isDouble() ? v.toDouble() : dst;
    };
    const auto integ = [&o](const char *key, int dst) {
        const auto v = o.value(QLatin1String(key));
        return v.isDouble() ? int(v.toDouble()) : dst;
    };

    in.capacity_kVA = num("capacity_kVA", in.capacity_kVA);
    in.hvRated_kV = num("hvRated_kV", in.hvRated_kV);
    in.lvRated_kV = num("lvRated_kV", in.lvRated_kV);
    in.hvTapMax_pct = num("hvTapMax_pct", in.hvTapMax_pct);
    in.hvTapMin_pct = num("hvTapMin_pct", in.hvTapMin_pct);
    in.hvDeltaConnected = o.value(QStringLiteral("hvDeltaConnected")).toBool(in.hvDeltaConnected);
    in.lvStarConnected = o.value(QStringLiteral("lvStarConnected")).toBool(in.lvStarConnected);

    in.coreDiameter_mm = num("coreDiameter_mm", in.coreDiameter_mm);
    in.coreStraight_mm = num("coreStraight_mm", in.coreStraight_mm);
    in.ellipseAngle_deg = num("ellipseAngle_deg", in.ellipseAngle_deg);
    in.stackFactor = num("stackFactor", in.stackFactor);
    in.steelThickness_mm = num("steelThickness_mm", in.steelThickness_mm);
    in.steelGrade = o.value(QStringLiteral("steelGrade")).toString(in.steelGrade);
    in.seamCount = integ("seamCount", in.seamCount);
    in.coreLossCraftCoef = num("coreLossCraftCoef", in.coreLossCraftCoef);
    in.yokePiece1Stack_mm = num("yokePiece1Stack_mm", in.yokePiece1Stack_mm);
    in.yokePiece2Stack_mm = num("yokePiece2Stack_mm", in.yokePiece2Stack_mm);
    in.yokePiece3Stack_mm = num("yokePiece3Stack_mm", in.yokePiece3Stack_mm);
    in.yokeWidenTo_mm = num("yokeWidenTo_mm", in.yokeWidenTo_mm);
    in.yokeWidenStages = integ("yokeWidenStages", in.yokeWidenStages);
    const QJsonArray steps = o.value(QStringLiteral("yokeSteps_mm")).toArray();
    for (int i = 0; i < 16 && i < steps.size(); ++i) {
        in.yokeSteps_mm[i] = steps.at(i).toDouble(in.yokeSteps_mm[i]);
    }

    in.hvBareWidth_mm = num("hvBareWidth_mm", in.hvBareWidth_mm);
    in.hvBareThick_mm = num("hvBareThick_mm", in.hvBareThick_mm);
    in.hvParallelCount = integ("hvParallelCount", in.hvParallelCount);
    in.hvStackCount = integ("hvStackCount", in.hvStackCount);
    in.hvTurnsPerLayer = integ("hvTurnsPerLayer", in.hvTurnsPerLayer);
    in.hvLayerInsul_mm = num("hvLayerInsul_mm", in.hvLayerInsul_mm);
    in.hvWireInsulAdd_mm = num("hvWireInsulAdd_mm", in.hvWireInsulAdd_mm);
    in.hvCoilFormIdx = integ("hvCoilFormIdx", in.hvCoilFormIdx);
    const QJsonArray dws = o.value(QStringLiteral("hvDuctWidthSide")).toArray();
    const QJsonArray dhs = o.value(QStringLiteral("hvDuctHeightSide")).toArray();
    for (int i = 0; i < 5; ++i) {
        if (i < dws.size()) {
            in.hvDuctWidthSide[i] = dws.at(i).toDouble(in.hvDuctWidthSide[i]);
        }
        if (i < dhs.size()) {
            in.hvDuctHeightSide[i] = dhs.at(i).toDouble(in.hvDuctHeightSide[i]);
        }
    }

    in.lvTurns = integ("lvTurns", in.lvTurns);
    in.lvFoilThick_mm = num("lvFoilThick_mm", in.lvFoilThick_mm);
    in.lvFoilWidth_mm = num("lvFoilWidth_mm", in.lvFoilWidth_mm);
    in.lvLayerInsulCount = integ("lvLayerInsulCount", in.lvLayerInsulCount);
    in.lvLayerInsul_mm = num("lvLayerInsul_mm", in.lvLayerInsul_mm);
    in.lvEndInsul_mm = num("lvEndInsul_mm", in.lvEndInsul_mm);
    in.lvCopperFoil = o.value(QStringLiteral("lvCopperFoil")).toBool(in.lvCopperFoil);
    const QJsonArray lws = o.value(QStringLiteral("lvDuctWidthSide")).toArray();
    const QJsonArray lhs = o.value(QStringLiteral("lvDuctHeightSide")).toArray();
    for (int i = 0; i < 5; ++i) {
        if (i < lws.size()) {
            in.lvDuctWidthSide[i] = lws.at(i).toDouble(in.lvDuctWidthSide[i]);
        }
        if (i < lhs.size()) {
            in.lvDuctHeightSide[i] = lhs.at(i).toDouble(in.lvDuctHeightSide[i]);
        }
    }

    in.mainDuctWidth_mm = num("mainDuctWidth_mm", in.mainDuctWidth_mm);
    in.mainDuctInsul_mm = num("mainDuctInsul_mm", in.mainDuctInsul_mm);
    in.strayLossFactor = num("strayLossFactor", in.strayLossFactor);
    in.leadLoss_W = num("leadLoss_W", in.leadLoss_W);
    in.lvExtraLoss_W = num("lvExtraLoss_W", in.lvExtraLoss_W);

    in.tankBottomOil_mm = num("tankBottomOil_mm", in.tankBottomOil_mm);
    in.tankSideClear_mm = num("tankSideClear_mm", in.tankSideClear_mm);
    in.tankEndClear_mm = num("tankEndClear_mm", in.tankEndClear_mm);
    in.tankFoot_mm = num("tankFoot_mm", in.tankFoot_mm);
    in.waveDepth_mm = integ("waveDepth_mm", in.waveDepth_mm);
    in.waveHeight_mm = integ("waveHeight_mm", in.waveHeight_mm);
    in.wavePitch_mm = integ("wavePitch_mm", in.wavePitch_mm);
    in.phaseGapBase_mm = num("phaseGapBase_mm", in.phaseGapBase_mm);
    in.refFluxDens_T = num("refFluxDens_T", in.refFluxDens_T);
    return in;
}

// ---- 方案库文件读写 ----
// 保存全部方案的设计变量；返回是否成功
inline bool saveSchemes(const QString &path, const QVector<CalcInput> &inputs,
                        const QString &note = QString())
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("ZTBLD-SchemeStore"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("savedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!note.isEmpty()) {
        root.insert(QStringLiteral("note"), note);
    }
    QJsonArray arr;
    for (const CalcInput &in : inputs) {
        arr.append(toJson(in));
    }
    root.insert(QStringLiteral("schemes"), arr);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// 加载方案库：解析失败或格式不对返回空并置 ok=false
inline QVector<CalcInput> loadSchemes(const QString &path, bool *ok = nullptr)
{
    if (ok) {
        *ok = false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("ZTBLD-SchemeStore")) {
        return {};
    }
    const QJsonArray arr = root.value(QStringLiteral("schemes")).toArray();
    QVector<CalcInput> inputs;
    inputs.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (v.isObject()) {
            inputs.append(fromJson(v.toObject()));
        }
    }
    if (ok) {
        *ok = true;
    }
    return inputs;
}

// ---- 带元数据的方案条目（我的方案库/记忆库/上次方案共用）----
// 试验实测数据（AI 自学习第一步：设计值 vs 实测值对比的数据源）
struct TestReport {
    bool hasData = false;          // 是否已录入实测数据
    QDateTime testedAt;            // 试验日期
    double noLoadLoss_W = 0.0;     // 实测空载损耗
    double loadLoss_W = 0.0;       // 实测负载损耗
    double impedance_pct = 0.0;    // 实测阻抗电压
    double oilTopRise_K = 0.0;     // 实测油顶层温升
    double hvWindingRise_K = 0.0;  // 实测高压绕组温升
    double lvWindingRise_K = 0.0;  // 实测低压绕组温升
};

// 试验数据 JSON 序列化（空值全部容错，旧方案文件不含此字段即无实测数据）
inline QJsonObject testReportToJson(const TestReport &t)
{
    QJsonObject o;
    o.insert(QStringLiteral("hasData"), t.hasData);
    o.insert(QStringLiteral("testedAt"), t.testedAt.toString(Qt::ISODate));
    o.insert(QStringLiteral("noLoadLoss_W"), t.noLoadLoss_W);
    o.insert(QStringLiteral("loadLoss_W"), t.loadLoss_W);
    o.insert(QStringLiteral("impedance_pct"), t.impedance_pct);
    o.insert(QStringLiteral("oilTopRise_K"), t.oilTopRise_K);
    o.insert(QStringLiteral("hvWindingRise_K"), t.hvWindingRise_K);
    o.insert(QStringLiteral("lvWindingRise_K"), t.lvWindingRise_K);
    return o;
}

inline TestReport testReportFromJson(const QJsonObject &o)
{
    TestReport t;
    t.hasData = o.value(QStringLiteral("hasData")).toBool(false);
    t.testedAt = QDateTime::fromString(
        o.value(QStringLiteral("testedAt")).toString(), Qt::ISODate);
    const auto num = [&o](const char *key) {
        const auto v = o.value(QLatin1String(key));
        return v.isDouble() ? v.toDouble() : 0.0;
    };
    t.noLoadLoss_W = num("noLoadLoss_W");
    t.loadLoss_W = num("loadLoss_W");
    t.impedance_pct = num("impedance_pct");
    t.oilTopRise_K = num("oilTopRise_K");
    t.hvWindingRise_K = num("hvWindingRise_K");
    t.lvWindingRise_K = num("lvWindingRise_K");
    return t;
}

struct SchemeEntry {
    QString name;         // 方案名称（记忆库为自动生成的时间戳名）
    QDateTime savedAt;    // 保存/使用时间
    CalcInput input;      // 设计变量
    TestReport test;      // 试验实测数据（AI 自学习对比用，可空）
};

// 条目序列化：设计变量字段之上叠加 name/savedAt/实测数据元数据
inline QJsonObject entryToJson(const SchemeEntry &e)
{
    QJsonObject o = toJson(e.input);
    o.insert(QStringLiteral("name"), e.name);
    o.insert(QStringLiteral("savedAt"), e.savedAt.toString(Qt::ISODate));
    if (e.test.hasData) {
        o.insert(QStringLiteral("test"), testReportToJson(e.test));
    }
    return o;
}

inline SchemeEntry entryFromJson(const QJsonObject &o)
{
    SchemeEntry e;
    e.name = o.value(QStringLiteral("name")).toString();
    e.savedAt = QDateTime::fromString(
        o.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
    e.input = fromJson(o);
    e.test = testReportFromJson(o.value(QStringLiteral("test")).toObject());
    return e;
}

// ---- 三类持久化库（AppData/schemes 目录）----
// AppData 下方案存储目录中的文件路径（目录不存在则创建）
inline QString storePath(const QString &fileName)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/schemes");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/") + fileName;
}

// 我的方案库（用户命名保存，可增删）
inline QString mySchemesPath()     { return storePath(QStringLiteral("my_schemes.json")); }
// 记忆库（自动记录最近使用的方案，去重限量）
inline QString memorySchemesPath() { return storePath(QStringLiteral("memory_schemes.json")); }
// 上次方案（单条，进入计算时覆盖）
inline QString lastSchemePath()    { return storePath(QStringLiteral("last_scheme.json")); }

// 列表库读写（我的方案库/记忆库共用；format 区别于旧版纯数组方案库文件）
inline bool saveEntries(const QString &path, const QVector<SchemeEntry> &entries)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("ZTBLD-SchemeEntries"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("savedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    QJsonArray arr;
    for (const SchemeEntry &e : entries) {
        arr.append(entryToJson(e));
    }
    root.insert(QStringLiteral("schemes"), arr);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

inline QVector<SchemeEntry> loadEntries(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("ZTBLD-SchemeEntries")) {
        return {};
    }
    const QJsonArray arr = root.value(QStringLiteral("schemes")).toArray();
    QVector<SchemeEntry> entries;
    entries.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (v.isObject()) {
            entries.append(entryFromJson(v.toObject()));
        }
    }
    return entries;
}

// 设计变量是否完全一致（记忆库去重依据；借 JSON 序列化比较，免手写字段比对）
inline bool sameInput(const CalcInput &a, const CalcInput &b)
{
    return toJson(a) == toJson(b);
}

// 记忆库追加：设计变量相同则移除旧记录后重新置顶（更新时间），保持最近 maxCount 条
inline void appendMemory(const CalcInput &input, int maxCount = 20)
{
    QVector<SchemeEntry> entries = loadEntries(memorySchemesPath());
    for (int i = 0; i < entries.size(); ++i) {
        if (sameInput(entries[i].input, input)) {
            entries.removeAt(i);
            break;
        }
    }
    SchemeEntry e;
    e.name = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    e.savedAt = QDateTime::currentDateTime();
    e.input = input;
    entries.prepend(e);
    while (entries.size() > maxCount) {
        entries.removeLast();
    }
    saveEntries(memorySchemesPath(), entries);
}

// 上次方案（单条覆盖写）
inline bool saveLastScheme(const CalcInput &input)
{
    SchemeEntry e;
    e.name = QStringLiteral("上次方案");
    e.savedAt = QDateTime::currentDateTime();
    e.input = input;
    return saveEntries(lastSchemePath(), { e });
}

inline bool loadLastScheme(CalcInput &out)
{
    const QVector<SchemeEntry> entries = loadEntries(lastSchemePath());
    if (entries.isEmpty()) {
        return false;
    }
    out = entries.first().input;
    return true;
}

} // namespace SchemeStore

#endif // SCHEMESTORE_H
