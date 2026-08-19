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

} // namespace SchemeStore

#endif // SCHEMESTORE_H
