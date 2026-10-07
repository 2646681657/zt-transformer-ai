#ifndef IOPTIMIZER_H
#define IOPTIMIZER_H
// 优化器接口（异步执行多方案寻优，通过信号报告进度和候选方案）

#include <QObject>
#include <QVector>
#include <QMetaType>
#include <QMap>
#include <cmath>
#include <limits>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "CalcInput.h"
#include "CalcResult.h"
#include "OptimizationResult.h"

struct OptimizationSettings {
    enum Method { Optimize, Exhaustive };
    Method method = Exhaustive; // 保留扩展接口；当前仅单轮网格
    int threadCount = 1;        // 当前仅一个后台计算线程
    enum CostModel { CuFe, CuFeOil };
    CostModel costModel = CuFeOil;
    // 网格搜索范围（围绕基准设计变量的 ±N 步）与步长
    double diaStep_mm = 5.0;      // 铁芯直径步进 mm
    int diaRange = 2;             // 直径 ±2 步（5 档）
    double straightStep_mm = 5.0; // 直线段长步进 mm
    int straightRange = 1;        // 直线段 ±1 步（3 档）
    int lvTurnsRange = 1;         // 低压匝数 ±1（3 档）
    int hvTplRange = 1;           // 高压总层数 W12 ±1（3 档），保留旧字段名
    bool searchDiameter = true;
    bool searchStraight = true;
    bool searchLvTurns = true;
    bool searchHvLayers = true;

    int diameterRadius() const { return searchDiameter ? diaRange : 0; }
    int straightRadius() const { return searchStraight ? straightRange : 0; }
    int lvTurnsRadius() const { return searchLvTurns ? lvTurnsRange : 0; }
    int hvLayersRadius() const { return searchHvLayers ? hvTplRange : 0; }
    int plannedCount() const {
        const int ranges[] = {diameterRadius(), straightRadius(), lvTurnsRadius(), hvLayersRadius()};
        int count = 1;
        for (int range : ranges) {
            if (range < 0 || range > 5) return 0;
            count *= 2 * range + 1;
        }
        return count;
    }
    QString validationError(const CalcInput &base) const {
        if (plannedCount() == 0)
            return QStringLiteral("参与寻优的范围必须为0至5步。");
        const auto validStep = [](double step) { return std::isfinite(step) && step >= 1.0 && step <= 50.0; };
        if ((searchDiameter && !validStep(diaStep_mm)) || (searchStraight && !validStep(straightStep_mm)))
            return QStringLiteral("参与寻优的尺寸步长必须为1至50 mm。");
        const double d = base.coreDiameter_mm - diameterRadius() * (searchDiameter ? diaStep_mm : 0.0);
        const double l = base.coreStraight_mm - straightRadius() * (searchStraight ? straightStep_mm : 0.0);
        if (!std::isfinite(d) || d <= 0 || !std::isfinite(l) || l < 0
                || base.lvTurns <= lvTurnsRadius() || base.hvTurnsPerLayer < hvLayersRadius() + 2)
            return QStringLiteral("搜索下界无效：铁芯直径须>0，直线段须≥0，低压匝数须≥1，高压总层数W12须≥2。请缩小范围或修改基准。");
        if (base.lvTurns > std::numeric_limits<int>::max() - lvTurnsRadius()
                || base.hvTurnsPerLayer > std::numeric_limits<int>::max() - hvLayersRadius()
                || !std::isfinite(base.coreDiameter_mm + diameterRadius() * (searchDiameter ? diaStep_mm : 0.0))
                || !std::isfinite(base.coreStraight_mm + straightRadius() * (searchStraight ? straightStep_mm : 0.0)))
            return QStringLiteral("搜索上界超出可计算数值范围。");
        return {};
    }
};

struct OptimizationRunSummary {
    int planned = 0;
    int evaluated = 0;
    int accepted = 0;
    int invalid = 0;
    int constraintRejected = 0;
    qint64 elapsed_ms = 0; // 墙钟时间，包含暂停
    QString error;
    QStringList skippedChecks;
    QMap<QString, int> rejectionReasons;
    QMap<QString, int> calculationErrors; // 最多20种失败描述，其余归入其他
};
Q_DECLARE_METATYPE(OptimizationRunSummary)

// 寻优候选方案：完整输入/输出 + 方案表行数据
struct OptimizeCandidate {
    CalcInput input;
    CalcResult result;
    OptimizationResult scheme;
};
Q_DECLARE_METATYPE(OptimizeCandidate)

class IOptimizer : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IOptimizer() = default;

    // 启动优化计算（异步）：围绕 baseInput 设计变量搜索，
    // 候选方案通过 candidateReady 信号逐个返回，结束发 finished
    virtual void start(const TransformerParams &params,
                       const StructureConfig &config,
                       const CalcInput &baseInput,
                       const OptimizationSettings &settings) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;

signals:
    void progressUpdated(int percent);
    void candidateReady(const OptimizeCandidate &candidate);
    // 寻优结束（stopped=true 表示被手动停止）；
    // accepted>0 时 best 为已评估候选中材料成本最低的方案。
    void finished(bool stopped, const OptimizeCandidate &best, const OptimizationRunSummary &summary);
};

#endif // IOPTIMIZER_H
