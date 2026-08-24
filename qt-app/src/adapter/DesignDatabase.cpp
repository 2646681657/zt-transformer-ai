#include "DesignDatabase.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

DesignDatabase &DesignDatabase::instance()
{
    static DesignDatabase db;
    return db;
}

bool DesignDatabase::load(const QString &resourcePath)
{
    if (m_loaded) {
        return true;
    }

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("无法打开数据资源: %1").arg(resourcePath);
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("JSON 解析失败: %1").arg(err.errorString());
        return false;
    }
    const QJsonObject root = doc.object();

    // 硅钢性能曲线
    m_steelCurves.clear();
    const QJsonArray grades = root.value(QStringLiteral("siliconSteel")).toObject()
                                  .value(QStringLiteral("grades")).toArray();
    for (const auto &gv : grades) {
        const QJsonObject go = gv.toObject();
        SteelCurve curve;
        curve.grade = go.value(QStringLiteral("grade")).toString();
        const QJsonArray pts = go.value(QStringLiteral("points")).toArray();
        for (const auto &pv : pts) {
            const QJsonObject po = pv.toObject();
            SteelCurvePoint pt;
            pt.b = po.value(QStringLiteral("b")).toDouble();
            pt.wPerKg = po.value(QStringLiteral("wPerKg")).toDouble();
            pt.vaPerKg = po.value(QStringLiteral("vaPerKg")).toDouble();
            pt.index = po.value(QStringLiteral("index")).toInt();
            curve.points.append(pt);
        }
        m_steelCurves.append(curve);
    }

    // 性能标准值（容量 → 空载电流%）
    m_perfStd.clear();
    const QJsonArray perf = root.value(QStringLiteral("performanceStandards")).toArray();
    for (const auto &v : perf) {
        const QJsonObject o = v.toObject();
        m_perfStd.append({o.value(QStringLiteral("capacityKVA")).toDouble(),
                          o.value(QStringLiteral("noLoadCurrentPct")).toDouble()});
    }

    // 铁芯叠积对照表
    m_coreRows.clear();
    const QJsonArray core = root.value(QStringLiteral("coreLaminations")).toArray();
    for (const auto &v : core) {
        const QJsonObject o = v.toObject();
        CoreLaminationRow row;
        row.ref = o.value(QStringLiteral("ref")).toDouble();
        for (const auto &w : o.value(QStringLiteral("widths")).toArray()) {
            row.widths.append(w.toDouble());
        }
        for (const auto &y : o.value(QStringLiteral("yokeBase")).toArray()) {
            row.yokeBase.append(y.toDouble());
        }
        m_coreRows.append(row);
    }

    // 波纹油箱系数
    m_corrCoefs.clear();
    const QJsonArray corr = root.value(QStringLiteral("corrugatedTankCoefficients")).toArray();
    for (const auto &v : corr) {
        const QJsonObject o = v.toObject();
        m_corrCoefs.append({o.value(QStringLiteral("depthMm")).toDouble(),
                            o.value(QStringLiteral("ks")).toDouble(),
                            o.value(QStringLiteral("kp")).toDouble()});
    }

    // 线规表
    m_wireSpecs.clear();
    const QJsonArray wires = root.value(QStringLiteral("wireSpecs")).toArray();
    for (const auto &v : wires) {
        const QJsonObject o = v.toObject();
        WireSpec spec;
        spec.bareWidthMm = o.value(QStringLiteral("bareWidthMm")).toDouble();
        spec.insulatedWidthMm = o.value(QStringLiteral("insulatedWidthMm")).toDouble();
        spec.weightAddPct = o.value(QStringLiteral("weightAddPct")).toDouble();
        m_wireSpecs.append(spec);
    }

    m_loaded = true;
    m_lastError.clear();
    return true;
}

const SteelCurve *DesignDatabase::findSteel(const QString &grade) const
{
    const QString g = grade.trimmed();
    for (const auto &curve : m_steelCurves) {
        if (curve.grade.compare(g, Qt::CaseInsensitive) == 0) {
            return &curve;
        }
    }
    return nullptr;
}

bool DesignDatabase::steelGradeExists(const QString &grade) const
{
    return findSteel(grade) != nullptr;
}

namespace {
// 阶梯查找：在升序 keys 中取 ≤key 的最大项，返回其索引（Excel LOOKUP 语义）
// 无匹配（key 小于最小键或键为空）返回 -1
int stepIndex(const QVector<double> &keys, double key)
{
    if (keys.isEmpty() || key < keys.first()) {
        return -1;
    }
    // upper_bound 找第一个 > key 的位置，其前一个即 ≤key 的最大项
    const auto it = std::upper_bound(keys.begin(), keys.end(), key);
    return int(it - keys.begin()) - 1;
}
}  // namespace

bool DesignDatabase::steelLossPerKg(const QString &grade, double bT, double &wPerKg) const
{
    const SteelCurve *curve = findSteel(grade);
    if (!curve) {
        return false;
    }
    QVector<double> keys;
    keys.reserve(curve->points.size());
    for (const auto &pt : curve->points) {
        keys.append(pt.b);
    }
    const int i = stepIndex(keys, bT);
    if (i < 0) {
        return false;
    }
    wPerKg = curve->points[i].wPerKg;
    return true;
}

bool DesignDatabase::steelMagnetizationPerKg(const QString &grade, double bT,
                                             double &vaPerKg) const
{
    const SteelCurve *curve = findSteel(grade);
    if (!curve) {
        return false;
    }
    QVector<double> keys;
    keys.reserve(curve->points.size());
    for (const auto &pt : curve->points) {
        keys.append(pt.b);
    }
    const int i = stepIndex(keys, bT);
    if (i < 0) {
        return false;
    }
    vaPerKg = curve->points[i].vaPerKg;
    return true;
}

bool DesignDatabase::steelBFromLossPerKg(const QString &grade, double wPerKg, double &bT) const
{
    const SteelCurve *curve = findSteel(grade);
    if (!curve) {
        return false;
    }
    QVector<double> keys;  // 铁损随磁密单调升，可作升序键
    keys.reserve(curve->points.size());
    for (const auto &pt : curve->points) {
        keys.append(pt.wPerKg);
    }
    const int i = stepIndex(keys, wPerKg);
    if (i < 0) {
        return false;
    }
    bT = curve->points[i].b;
    return true;
}

bool DesignDatabase::steelIndexOfB(const QString &grade, double bT, int &index) const
{
    const SteelCurve *curve = findSteel(grade);
    if (!curve) {
        return false;
    }
    QVector<double> keys;
    keys.reserve(curve->points.size());
    for (const auto &pt : curve->points) {
        keys.append(pt.b);
    }
    const int i = stepIndex(keys, bT);
    if (i < 0) {
        return false;
    }
    index = curve->points[i].index;
    return true;
}

bool DesignDatabase::steelLossPerKgInterp(const QString &grade, double bT, double &wPerKg) const
{
    const SteelCurve *curve = findSteel(grade);
    if (!curve || curve->points.isEmpty()) {
        return false;
    }
    const auto &pts = curve->points;
    if (bT <= pts.first().b) {
        wPerKg = pts.first().wPerKg;
        return true;
    }
    if (bT >= pts.last().b) {
        wPerKg = pts.last().wPerKg;
        return true;
    }
    for (int i = 1; i < pts.size(); ++i) {
        if (bT <= pts[i].b) {
            const auto &p0 = pts[i - 1];
            const auto &p1 = pts[i];
            const double t = (bT - p0.b) / (p1.b - p0.b);
            wPerKg = p0.wPerKg + t * (p1.wPerKg - p0.wPerKg);
            return true;
        }
    }
    wPerKg = pts.last().wPerKg;
    return true;
}

bool DesignDatabase::steelMagnetizationPerKgInterp(const QString &grade, double bT,
                                                   double &vaPerKg) const
{
    const SteelCurve *curve = findSteel(grade);
    if (!curve || curve->points.isEmpty()) {
        return false;
    }
    const auto &pts = curve->points;
    if (bT <= pts.first().b) {
        vaPerKg = pts.first().vaPerKg;
        return true;
    }
    if (bT >= pts.last().b) {
        vaPerKg = pts.last().vaPerKg;
        return true;
    }
    for (int i = 1; i < pts.size(); ++i) {
        if (bT <= pts[i].b) {
            const auto &p0 = pts[i - 1];
            const auto &p1 = pts[i];
            const double t = (bT - p0.b) / (p1.b - p0.b);
            vaPerKg = p0.vaPerKg + t * (p1.vaPerKg - p0.vaPerKg);
            return true;
        }
    }
    vaPerKg = pts.last().vaPerKg;
    return true;
}

bool DesignDatabase::noLoadCurrentStd(double capacityKVA, double &pct) const
{
    QVector<double> keys;
    keys.reserve(m_perfStd.size());
    for (const auto &p : m_perfStd) {
        keys.append(p.first);
    }
    const int i = stepIndex(keys, capacityKVA);
    if (i < 0) {
        return false;
    }
    pct = m_perfStd[i].second;
    return true;
}

bool DesignDatabase::coreWidths(double ref, QVector<double> &widths) const
{
    QVector<double> keys;
    keys.reserve(m_coreRows.size());
    for (const auto &row : m_coreRows) {
        keys.append(row.ref);
    }
    const int i = stepIndex(keys, ref);
    if (i < 0) {
        return false;
    }
    widths = m_coreRows[i].widths;
    return true;
}

bool DesignDatabase::coreYokeBase(double ref, QVector<double> &yokeBase) const
{
    QVector<double> keys;
    keys.reserve(m_coreRows.size());
    for (const auto &row : m_coreRows) {
        keys.append(row.ref);
    }
    const int i = stepIndex(keys, ref);
    if (i < 0) {
        return false;
    }
    yokeBase = m_coreRows[i].yokeBase;
    return true;
}

bool DesignDatabase::corrugatedCoefs(double depthMm, double &ks, double &kp) const
{
    QVector<double> keys;
    keys.reserve(m_corrCoefs.size());
    for (const auto &c : m_corrCoefs) {
        keys.append(c[0]);
    }
    const int i = stepIndex(keys, depthMm);
    if (i < 0) {
        return false;
    }
    ks = m_corrCoefs[i][1];
    kp = m_corrCoefs[i][2];
    return true;
}

bool DesignDatabase::wireByBareWidth(double bareWidthMm, WireSpec &spec) const
{
    QVector<double> keys;
    keys.reserve(m_wireSpecs.size());
    for (const auto &w : m_wireSpecs) {
        keys.append(w.bareWidthMm);
    }
    const int i = stepIndex(keys, bareWidthMm);
    if (i < 0) {
        return false;
    }
    spec = m_wireSpecs[i];
    return true;
}

// ============================================================================
// 数据导出/导入
// ============================================================================

QJsonDocument DesignDatabase::exportToJson() const
{
    QJsonObject root;

    // 硅钢性能曲线
    QJsonObject steelObj;
    QJsonArray grades;
    for (const auto &curve : m_steelCurves) {
        QJsonObject go;
        go.insert("grade", curve.grade);
        QJsonArray pts;
        for (const auto &pt : curve.points) {
            QJsonObject po;
            po.insert("b", pt.b);
            po.insert("wPerKg", pt.wPerKg);
            po.insert("vaPerKg", pt.vaPerKg);
            po.insert("index", pt.index);
            pts.append(po);
        }
        go.insert("points", pts);
        grades.append(go);
    }
    steelObj.insert("grades", grades);
    root.insert("siliconSteel", steelObj);

    // 性能标准值
    QJsonArray perf;
    for (const auto &p : m_perfStd) {
        QJsonObject o;
        o.insert("capacityKVA", p.first);
        o.insert("noLoadCurrentPct", p.second);
        perf.append(o);
    }
    root.insert("performanceStandards", perf);

    // 铁芯叠积对照表
    QJsonArray core;
    for (const auto &row : m_coreRows) {
        QJsonObject o;
        o.insert("ref", row.ref);
        QJsonArray w;
        for (double v : row.widths) w.append(v);
        o.insert("widths", w);
        QJsonArray y;
        for (double v : row.yokeBase) y.append(v);
        o.insert("yokeBase", y);
        core.append(o);
    }
    root.insert("coreLaminations", core);

    // 波纹油箱系数
    QJsonArray corr;
    for (const auto &c : m_corrCoefs) {
        QJsonObject o;
        o.insert("depthMm", c[0]);
        o.insert("ks", c[1]);
        o.insert("kp", c[2]);
        corr.append(o);
    }
    root.insert("corrugatedTankCoefficients", corr);

    // 线规表
    QJsonArray wires;
    for (const auto &spec : m_wireSpecs) {
        QJsonObject o;
        o.insert("bareWidthMm", spec.bareWidthMm);
        o.insert("insulatedWidthMm", spec.insulatedWidthMm);
        o.insert("weightAddPct", spec.weightAddPct);
        wires.append(o);
    }
    root.insert("wireSpecs", wires);

    return QJsonDocument(root);
}

QString DesignDatabase::exportToCsv(const QString &category) const
{
    QString out;

    if (category == "steel") {
        out = "grade,b,wPerKg,vaPerKg,index\n";
        for (const auto &curve : m_steelCurves) {
            for (const auto &pt : curve.points) {
                out += QString("%1,%2,%3,%4,%5\n")
                          .arg(curve.grade)
                          .arg(pt.b, 0, 'f', 3)
                          .arg(pt.wPerKg, 0, 'f', 4)
                          .arg(pt.vaPerKg, 0, 'f', 4)
                          .arg(pt.index);
            }
        }
    } else if (category == "perf") {
        out = "capacityKVA,noLoadCurrentPct\n";
        for (const auto &p : m_perfStd) {
            out += QString("%1,%2\n").arg(p.first, 0, 'f', 0).arg(p.second, 0, 'f', 2);
        }
    } else if (category == "core") {
        out = "ref,width1,width2,width3,width4,width5,width6,width7,"
              "width8,width9,width10,width11,yoke1,yoke2,yoke3,yoke4\n";
        for (const auto &row : m_coreRows) {
            out += QString::number(row.ref, 'f', 0);
            for (int i = 0; i < 11 && i < row.widths.size(); ++i)
                out += "," + QString::number(row.widths[i], 'f', 0);
            for (int i = row.widths.size(); i < 11; ++i)
                out += ",0";
            for (int i = 0; i < 4 && i < row.yokeBase.size(); ++i)
                out += "," + QString::number(row.yokeBase[i], 'f', 0);
            for (int i = row.yokeBase.size(); i < 4; ++i)
                out += ",0";
            out += "\n";
        }
    } else if (category == "corrugated") {
        out = "depthMm,ks,kp\n";
        for (const auto &c : m_corrCoefs) {
            out += QString("%1,%2,%3\n")
                      .arg(c[0], 0, 'f', 0)
                      .arg(c[1], 0, 'f', 4)
                      .arg(c[2], 0, 'f', 4);
        }
    } else if (category == "wire") {
        out = "bareWidthMm,insulatedWidthMm,weightAddPct\n";
        for (const auto &spec : m_wireSpecs) {
            out += QString("%1,%2,%3\n")
                      .arg(spec.bareWidthMm, 0, 'f', 2)
                      .arg(spec.insulatedWidthMm, 0, 'f', 2)
                      .arg(spec.weightAddPct, 0, 'f', 1);
        }
    }

    return out;
}

bool DesignDatabase::loadFromExternal(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("无法打开文件: %1").arg(filePath);
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("JSON 解析失败: %1").arg(err.errorString());
        return false;
    }

    // 复用 load() 的解析逻辑：先重置状态，再解析
    m_loaded = false;
    m_steelCurves.clear();
    m_perfStd.clear();
    m_coreRows.clear();
    m_corrCoefs.clear();
    m_wireSpecs.clear();

    const QJsonObject root = doc.object();

    // 硅钢性能曲线
    const QJsonArray gradesArr = root.value("siliconSteel").toObject()
                                     .value("grades").toArray();
    for (const auto &gv : gradesArr) {
        const QJsonObject go = gv.toObject();
        SteelCurve curve;
        curve.grade = go.value("grade").toString();
        const QJsonArray pts = go.value("points").toArray();
        for (const auto &pv : pts) {
            const QJsonObject po = pv.toObject();
            SteelCurvePoint pt;
            pt.b = po.value("b").toDouble();
            pt.wPerKg = po.value("wPerKg").toDouble();
            pt.vaPerKg = po.value("vaPerKg").toDouble();
            pt.index = po.value("index").toInt();
            curve.points.append(pt);
        }
        m_steelCurves.append(curve);
    }

    // 性能标准值
    const QJsonArray perfArr = root.value("performanceStandards").toArray();
    for (const auto &v : perfArr) {
        const QJsonObject o = v.toObject();
        m_perfStd.append({o.value("capacityKVA").toDouble(),
                          o.value("noLoadCurrentPct").toDouble()});
    }

    // 铁芯叠积对照表
    const QJsonArray coreArr = root.value("coreLaminations").toArray();
    for (const auto &v : coreArr) {
        const QJsonObject o = v.toObject();
        CoreLaminationRow row;
        row.ref = o.value("ref").toDouble();
        for (const auto &w : o.value("widths").toArray())
            row.widths.append(w.toDouble());
        for (const auto &y : o.value("yokeBase").toArray())
            row.yokeBase.append(y.toDouble());
        m_coreRows.append(row);
    }

    // 波纹油箱系数
    const QJsonArray corrArr = root.value("corrugatedTankCoefficients").toArray();
    for (const auto &v : corrArr) {
        const QJsonObject o = v.toObject();
        m_corrCoefs.append({o.value("depthMm").toDouble(),
                            o.value("ks").toDouble(),
                            o.value("kp").toDouble()});
    }

    // 线规表
    const QJsonArray wiresArr = root.value("wireSpecs").toArray();
    for (const auto &v : wiresArr) {
        const QJsonObject o = v.toObject();
        WireSpec spec;
        spec.bareWidthMm = o.value("bareWidthMm").toDouble();
        spec.insulatedWidthMm = o.value("insulatedWidthMm").toDouble();
        spec.weightAddPct = o.value("weightAddPct").toDouble();
        m_wireSpecs.append(spec);
    }

    m_loaded = true;
    m_lastError.clear();
    return true;
}
