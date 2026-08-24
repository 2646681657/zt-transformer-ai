#ifndef DESIGNDATABASE_H
#define DESIGNDATABASE_H
// 设计基础数据库：硅钢性能曲线 / 铁芯叠积对照 / 线规 / 波纹油箱系数等，
// 从内嵌资源 :/data/design_data.json 加载，替代 Excel 计算单中的 LOOKUP 查表

#include <QString>
#include <QVector>
#include <QJsonDocument>

struct SteelCurvePoint {
    double b = 0.0;        // 磁密 T
    double wPerKg = 0.0;   // 单位铁损 W/kg
    double vaPerKg = 0.0;  // 磁化容量 VA/kg
    int index = 0;         // 曲线序号（对应数据表 AL 列，1 起）
};

struct SteelCurve {
    QString grade;                 // 硅钢牌号（如 18SQGD065）
    QVector<SteelCurvePoint> points;  // 按磁密升序
};

struct CoreLaminationRow {
    double ref = 0.0;             // 参考直径/短轴长
    QVector<double> widths;       // 各级片宽（B..L 共 11 级，0 表示无）
    QVector<double> yokeBase;     // 轭片基数（M..P 共 4 列）
};

struct WireSpec {
    double bareWidthMm = 0.0;      // 裸线宽
    double insulatedWidthMm = 0.0; // 绝缘后宽
    double weightAddPct = 0.0;     // 导线加重量 %
};

class DesignDatabase {
public:
    // 全局单例（Engine/GUI 均可使用）
    static DesignDatabase &instance();

    // 从 qrc 资源加载；重复调用幂等（已加载且未出错则直接返回）
    bool load(const QString &resourcePath = QStringLiteral(":/data/design_data.json"));
    bool isLoaded() const { return m_loaded; }
    QString lastError() const { return m_lastError; }

    // ---- 硅钢性能曲线（阶梯查表：取 ≤key 的最大项，与 Excel LOOKUP 语义一致）----
    bool steelGradeExists(const QString &grade) const;
    // 磁密 → 单位铁损 W/kg
    bool steelLossPerKg(const QString &grade, double bT, double &wPerKg) const;
    // 磁密 → 磁化容量 VA/kg
    bool steelMagnetizationPerKg(const QString &grade, double bT, double &vaPerKg) const;
    // 单位铁损 → 磁密（反查，铁损随磁密单调升）
    bool steelBFromLossPerKg(const QString &grade, double wPerKg, double &bT) const;
    // 磁密 → 曲线序号（对应 LOOKUP(b, 磁密列, AL 序号)）
    bool steelIndexOfB(const QString &grade, double bT, int &index) const;
    const QVector<SteelCurve> &steelCurves() const { return m_steelCurves; }

    // ---- 硅钢性能曲线（线性插值：对应计算单 J16/I26 的两点内插）----
    // 磁密 → 单位铁损 W/kg（插值）
    bool steelLossPerKgInterp(const QString &grade, double bT, double &wPerKg) const;
    // 磁密 → 磁化容量 VA/kg（插值）
    bool steelMagnetizationPerKgInterp(const QString &grade, double bT, double &vaPerKg) const;

    // ---- 性能标准值 ----
    // 容量 → 空载电流标准 %
    bool noLoadCurrentStd(double capacityKVA, double &pct) const;

    // ---- 铁芯叠积对照表 ----
    // 参考值 → 各级片宽（阶梯查）
    bool coreWidths(double ref, QVector<double> &widths) const;
    // 参考值 → 轭片基数（M..P，使用时按 +90/+80/+60/+0 偏移）
    bool coreYokeBase(double ref, QVector<double> &yokeBase) const;

    // ---- 波纹油箱系数 ----
    // 波纹深 → Ks / Kp
    bool corrugatedCoefs(double depthMm, double &ks, double &kp) const;

    // ---- 线规表 ----
    // 裸线宽 → 线规（阶梯查）
    bool wireByBareWidth(double bareWidthMm, WireSpec &spec) const;

    // ---- 原始数据只读访问（数据查询页展示用）----
    const QVector<QPair<double, double>> &perfStandards() const { return m_perfStd; }
    const QVector<CoreLaminationRow> &coreRows() const { return m_coreRows; }
    const QVector<QVector<double>> &corrCoefs() const { return m_corrCoefs; }
    const QVector<WireSpec> &wireSpecs() const { return m_wireSpecs; }

    // ---- 数据导出/导入（数据管理工具用）----
    // 导出全部设计数据为 JSON 文档
    QJsonDocument exportToJson() const;
    // 导出指定类别为 CSV 文本（category: steel/perf/core/corrugated/wire）
    QString exportToCsv(const QString &category) const;
    // 从外部 JSON 文件导入数据（替换当前运行时数据，不影响 qrc 资源）
    bool loadFromExternal(const QString &filePath);

private:
    DesignDatabase() = default;

    const SteelCurve *findSteel(const QString &grade) const;

    bool m_loaded = false;
    QString m_lastError;
    QVector<SteelCurve> m_steelCurves;
    QVector<QPair<double, double>> m_perfStd;       // 容量 → 空载电流%
    QVector<CoreLaminationRow> m_coreRows;
    QVector<QVector<double>> m_corrCoefs;           // {depth, ks, kp}
    QVector<WireSpec> m_wireSpecs;
};

#endif // DESIGNDATABASE_H
