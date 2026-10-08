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
    enum Method { Optimize, Exhaustive, CoarseFine, MultiRound };
    Method method = Exhaustive; // 默认保留单轮网格；Optimize仍为预留值
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
    double lvFoilThickStep_mm = 0.05;
    int lvFoilThickRange = 1;
    double lvFoilWidthStep_mm = 5.0;
    int lvFoilWidthRange = 1;
    bool searchLvFoilThick = false; // 升级保留原135组合，新变量需显式开启
    bool searchLvFoilWidth = false;
    double hvBareWidthStep_mm = 0.05;
    int hvBareWidthRange = 1;
    double hvBareThickStep_mm = 0.05;
    int hvBareThickRange = 1;
    bool searchHvBareWidth = false;
    bool searchHvBareThick = false;
    bool searchHvRoundWire = false;
    int hvRoundWireRange = 1; // 表内相邻±N档，不是直径步长
    QVector<double> hvRoundWireDiameters; // 启动前按当前基准与表生成，不持久化
    QString hvRoundWireSelectionError;
    bool searchSteelGrade = false; // 默认固定基准牌号
    QStringList selectedSteelGrades; // 用户显式选择，持久化；不自动补入基准
    QStringList steelGrades; // 每次启动重新核对后生成的规范牌号快照
    QString steelGradeSelectionError;
    enum SteelPricing { BuiltInSteelPrice, CustomSteelPrices };
    SteelPricing steelPricing = BuiltInSteelPrice;
    QMap<QString, double> steelGradePrices; // 键为trim+大写；用户填写，候选生成时冻结
    QMap<QString, QString> steelGradePriceTexts; // 保留用户输入及空/非法值，设置持久化用
    MaterialPrices materialPrices;
    QStringList materialPriceTexts{QStringLiteral("60"), QStringLiteral("60"), QStringLiteral("10"), QStringLiteral("9")};
    static constexpr int maximumCombinations = 100000;
    static constexpr int fineSeedLimit = 3;
    static constexpr int maximumFineRounds = 5;
    static constexpr double improvementThreshold_pct = 0.01;
    static constexpr int stagnationRoundLimit = 2;
    int maxFineRounds = 3; // 仅多轮模式使用；粗搜不计入细搜轮数
    int fineRoundCount() const {
        return method == MultiRound ? maxFineRounds : (method == CoarseFine ? 1 : 0);
    }
    bool hasRefinement() const { return method == CoarseFine || method == MultiRound; }

    int diameterRadius() const { return searchDiameter ? diaRange : 0; }
    int straightRadius() const { return searchStraight ? straightRange : 0; }
    int lvTurnsRadius() const { return searchLvTurns ? lvTurnsRange : 0; }
    int hvLayersRadius() const { return searchHvLayers ? hvTplRange : 0; }
    int lvFoilThickRadius() const { return searchLvFoilThick ? lvFoilThickRange : 0; }
    int lvFoilWidthRadius() const { return searchLvFoilWidth ? lvFoilWidthRange : 0; }
    int hvBareWidthRadius() const { return searchHvBareWidth ? hvBareWidthRange : 0; }
    int hvBareThickRadius() const { return searchHvBareThick ? hvBareThickRange : 0; }
    int plannedCount() const {
        const int ranges[] = {diameterRadius(), straightRadius(), lvTurnsRadius(), hvLayersRadius(),
                              lvFoilThickRadius(), lvFoilWidthRadius(), hvBareWidthRadius(), hvBareThickRadius()};
        int count = 1;
        for (int range : ranges) {
            if (range < 0 || range > 5) return 0;
            count *= 2 * range + 1;
        }
        if (searchHvRoundWire) {
            if (hvRoundWireRange < 0 || hvRoundWireRange > 5 || hvRoundWireDiameters.isEmpty()
                    || hvRoundWireDiameters.size() > 2 * hvRoundWireRange + 1)
                return 0;
            if (count > std::numeric_limits<int>::max() / hvRoundWireDiameters.size())
                return std::numeric_limits<int>::max();
            count *= int(hvRoundWireDiameters.size());
        }
        if (searchSteelGrade) {
            if (steelGrades.isEmpty()) return 0;
            if (count > std::numeric_limits<int>::max() / steelGrades.size())
                return std::numeric_limits<int>::max();
            count *= int(steelGrades.size());
        }
        return count;
    }
    int fineNeighborhoodUpperBound() const {
        const int ranges[] = {diameterRadius(), straightRadius(), lvTurnsRadius(), hvLayersRadius(),
                              lvFoilThickRadius(), lvFoilWidthRadius(), hvBareWidthRadius(), hvBareThickRadius()};
        int count = 1;
        for (int range : ranges) count *= range > 0 ? 3 : 1;
        if (searchHvRoundWire) count *= qMin(3, int(hvRoundWireDiameters.size()));
        return count;
    }
    qint64 runUpperBound() const {
        const int coarse = plannedCount();
        const int rounds = fineRoundCount();
        // 第一轮中心最多为粗搜可行数；之后可从所有已评估候选中选3个。
        return qint64(coarse) + (rounds > 0
            ? (qint64(qMin(fineSeedLimit, coarse)) + qint64(rounds - 1) * fineSeedLimit)
                * fineNeighborhoodUpperBound() : 0);
    }
    QString validationError(const CalcInput &base) const {
        const auto materialPriceError = materialPrices.validationError();
        if (!materialPriceError.isEmpty()) return materialPriceError;
        if (steelPricing != BuiltInSteelPrice && steelPricing != CustomSteelPrices)
            return QStringLiteral("硅钢价模式无效，请重新选择内置基价或按牌号自定义价。");
        if (method != Exhaustive && method != CoarseFine && method != MultiRound)
            return QStringLiteral("不支持的寻优模式，请重新选择单轮网格、一轮细搜或多轮细化。");
        if (method == MultiRound && (maxFineRounds < 1 || maxFineRounds > maximumFineRounds))
            return QStringLiteral("多轮模式的最大细搜轮数须为1至%1，粗搜不计入该轮数。").arg(maximumFineRounds);
        if (searchSteelGrade) {
            if (!steelGradeSelectionError.isEmpty()) return steelGradeSelectionError;
            if (selectedSteelGrades.isEmpty() || steelGrades.size() != selectedSteelGrades.size())
                return QStringLiteral("请显式勾选至少一个有效硅钢牌号，并在启动前核对当前曲线数据。");
            QStringList identities;
            for (int i = 0; i < steelGrades.size(); ++i) {
                const QString identity = steelGrades[i].trimmed().toUpper();
                if (CalcInput::thicknessFromSteelGrade(steelGrades[i]) <= 0.0
                        || identities.contains(identity)
                        || identity != selectedSteelGrades[i].trimmed().toUpper())
                    return QStringLiteral("牌号快照须与显式选择一致、唯一且片厚格式有效，不允许静默替换。");
                identities.append(identity);
            }
        }
        if (steelPricing == CustomSteelPrices) {
            const auto grades = searchSteelGrade ? steelGrades : QStringList{base.steelGrade};
            for (const auto &grade : grades) {
                const double price = steelGradePrices.value(grade.trimmed().toUpper(), 0.0);
                if (!std::isfinite(price) || price <= 0.0 || price > 99999.0)
                    return QStringLiteral("牌号%1缺少有效自定义单价，请填写大于0、不超过99999元/kg的价格；不自动套用17元/kg或报价页价格。")
                        .arg(grade);
            }
        }
        if (searchHvRoundWire) {
            if (!base.isRoundHighVoltageWire())
                return QStringLiteral("当前为扁线，不能启用圆线规格寻优；请先在设计输入中选择有效圆线规格，或取消圆线参与。");
            if (!hvRoundWireSelectionError.isEmpty()) return hvRoundWireSelectionError;
            if (hvRoundWireRange < 0 || hvRoundWireRange > 5 || hvRoundWireDiameters.isEmpty()
                    || hvRoundWireDiameters.size() > 2 * hvRoundWireRange + 1)
                return QStringLiteral("圆线搜索档数须为0至5，且须先取得有效表内候选规格。");
            double previous = 0.0;
            for (double diameter : hvRoundWireDiameters) {
                if (!std::isfinite(diameter) || diameter <= previous)
                    return QStringLiteral("圆线候选直径必须大于零、有限、升序且唯一。");
                previous = diameter;
            }
            if (!hvRoundWireDiameters.contains(base.hvBareWidth_mm))
                return QStringLiteral("圆线候选规格必须包含当前基准直径，不允许取近似档替代。");
        }
        if (base.isRoundHighVoltageWire() && (searchHvBareWidth || searchHvBareThick))
            return QStringLiteral("当前为圆线，不能使用扁线宽/厚寻优。请取消高压裸线宽、厚的参与勾选；圆线仍只支持有效表内规格。");
        if (plannedCount() == 0)
            return QStringLiteral("参与寻优的范围必须为0至5步。");
        if (plannedCount() > maximumCombinations)
            return QStringLiteral("单轮最多%1组合，请缩小搜索范围或固定部分变量。").arg(maximumCombinations);
        if (runUpperBound() > maximumCombinations)
            return QStringLiteral("本次保守预算%1组合超过上限%2，请减少细搜轮数、缩小范围或固定部分变量（预算尚未扣除重复与边界裁剪）。")
                .arg(runUpperBound()).arg(maximumCombinations);
        const double wireBases[] = {base.hvBareWidth_mm, base.hvBareThick_mm};
        const double wireSteps[] = {searchHvBareWidth ? hvBareWidthStep_mm : 0.0,
                                    searchHvBareThick ? hvBareThickStep_mm : 0.0};
        const bool wireSearch[] = {searchHvBareWidth, searchHvBareThick};
        const int wireRanges[] = {hvBareWidthRadius(), hvBareThickRadius()};
        for (int i = 0; i < 2; ++i) {
            const double step = wireSteps[i];
            if (wireSearch[i] && (!std::isfinite(step) || step < 0.01 || step > 1.0))
                return QStringLiteral("高压扁线宽、厚步长须为0.01至1.00 mm。");
            const double span = wireRanges[i] * step;
            if (!std::isfinite(wireBases[i] - span) || wireBases[i] - span <= 0.0
                    || !std::isfinite(wireBases[i] + span))
                return QStringLiteral("高压裸线宽、厚的搜索下界须>0，上界须为有限值。请缩小范围或修改基准。");
            if (wireRanges[i] > 0 && (wireBases[i] + step == wireBases[i] || wireBases[i] - step == wireBases[i]))
                return QStringLiteral("高压裸线宽或厚步长小于当前数值的可表示精度，请核对基准与步长。");
        }
        if (!std::isfinite((wireBases[0] + wireRanges[0] * wireSteps[0])
                            * (wireBases[1] + wireRanges[1] * wireSteps[1])))
            return QStringLiteral("高压导线搜索截面积超出可计算数值范围。");
        const auto validStep = [](double step) { return std::isfinite(step) && step >= 1.0 && step <= 50.0; };
        if ((searchDiameter && !validStep(diaStep_mm)) || (searchStraight && !validStep(straightStep_mm)))
            return QStringLiteral("参与寻优的尺寸步长必须为1至50 mm。");
        if ((searchLvFoilThick && (!std::isfinite(lvFoilThickStep_mm)
                || lvFoilThickStep_mm < 0.01 || lvFoilThickStep_mm > 1.0))
                || (searchLvFoilWidth && !validStep(lvFoilWidthStep_mm)))
            return QStringLiteral("低压箔厚步长须为0.01至1.00 mm，箔宽步长须为1至50 mm。");
        const double thickSpan = lvFoilThickRadius() * (searchLvFoilThick ? lvFoilThickStep_mm : 0.0);
        const double widthSpan = lvFoilWidthRadius() * (searchLvFoilWidth ? lvFoilWidthStep_mm : 0.0);
        if (!std::isfinite(base.lvFoilThick_mm - thickSpan) || base.lvFoilThick_mm - thickSpan <= 0.0
                || !std::isfinite(base.lvFoilWidth_mm - widthSpan) || base.lvFoilWidth_mm - widthSpan <= 0.0
                || !std::isfinite(base.lvFoilThick_mm + thickSpan)
                || !std::isfinite(base.lvFoilWidth_mm + widthSpan)
                || !std::isfinite((base.lvFoilThick_mm + thickSpan) * (base.lvFoilWidth_mm + widthSpan)))
            return QStringLiteral("低压箔厚、箔宽的搜索下界须>0，上界及截面积须为有限值。请缩小范围或修改基准。");
        if ((lvFoilThickRadius() > 0 && (base.lvFoilThick_mm + lvFoilThickStep_mm == base.lvFoilThick_mm
                || base.lvFoilThick_mm - lvFoilThickStep_mm == base.lvFoilThick_mm))
                || (lvFoilWidthRadius() > 0 && (base.lvFoilWidth_mm + lvFoilWidthStep_mm == base.lvFoilWidth_mm
                || base.lvFoilWidth_mm - lvFoilWidthStep_mm == base.lvFoilWidth_mm)))
            return QStringLiteral("箔厚或箔宽步长小于当前数值的可表示精度，请核对基准与步长。");
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
        const double bases[] = {base.coreDiameter_mm, base.coreStraight_mm, base.lvFoilThick_mm,
                                base.lvFoilWidth_mm, base.hvBareWidth_mm, base.hvBareThick_mm};
        const double steps[] = {diaStep_mm, straightStep_mm, lvFoilThickStep_mm,
                                lvFoilWidthStep_mm, hvBareWidthStep_mm, hvBareThickStep_mm};
        const int radii[] = {diameterRadius(), straightRadius(), lvFoilThickRadius(),
                            lvFoilWidthRadius(), hvBareWidthRadius(), hvBareThickRadius()};
        for (int i = 0; i < 6; ++i) {
            const double delta = std::ldexp(steps[i], -fineRoundCount());
            const double upper = bases[i] + radii[i] * steps[i];
            const double lower = bases[i] - radii[i] * steps[i];
            if (radii[i] > 0 && (upper + delta == upper || upper - delta == upper
                    || lower + delta == lower || lower - delta == lower))
                return QStringLiteral("搜索步长小于当前尺寸的可表示精度，请核对基准与步长。");
        }
        return {};
    }
};

struct OptimizationStageSummary {
    int planned = 0;
    int evaluated = 0;
    int wireFormRejected = 0; // 扁线宽=厚预检剔除，未调用引擎，不算计算失败
    int accepted = 0;
    int invalid = 0;
    int constraintRejected = 0;
    qint64 elapsed_ms = 0; // 墙钟时间，包含暂停
    int processedCount() const { return evaluated + wireFormRejected; }
    void accumulate(const OptimizationStageSummary &other) {
        planned += other.planned;
        evaluated += other.evaluated;
        wireFormRejected += other.wireFormRejected;
        accepted += other.accepted;
        invalid += other.invalid;
        constraintRejected += other.constraintRejected;
        elapsed_ms += other.elapsed_ms;
    }
};

struct OptimizationRefinementSummary : OptimizationStageSummary {
    int round = 0;
    int seedCount = 0;
    int generated = 0;
    int duplicateSkipped = 0;
    bool completed = false;
    double bestCostBefore = 0.0;
    double bestCostAfter = 0.0;
    double improvement_pct = 0.0;
};

struct OptimizationSteelSummary : OptimizationStageSummary {
    bool hasBest = false;
    double bestCost = 0.0;
};

struct OptimizationRunSummary : OptimizationStageSummary {
    bool hasBest = false;
    int boundarySearchDimensions = 0; // 仅非零范围尺寸及至少两档的圆线
    QStringList bestBoundaryHits; // 已评估最低成本可行方案，相对本轮冻结范围
    OptimizationStageSummary coarse;
    OptimizationStageSummary fine;
    bool coarseCompleted = false;
    bool fineStarted = false;
    int fineSeedCount = 0;
    int fineGenerated = 0;
    int duplicateSkipped = 0;
    bool coarseHasBest = false;
    double coarseBestCost = 0.0;
    QString fineSkipReason;
    QVector<OptimizationRefinementSummary> refinements;
    QString stopReason; // 达到轮数/低改善/无新增组合/人工停止等，不作为全局收敛证明
    QMap<QString, OptimizationSteelSummary> steelSummaries; // 每个已规划牌号，含未处理/无可行
    QString error;
    QStringList skippedChecks;
    QMap<QString, int> rejectionReasons;
    QMap<QString, int> calculationErrors; // 最多20种失败描述，其余归入其他
    int processedCount() const { return evaluated + wireFormRejected; }
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
    void stageChanged(int stage, int planned); // 1粗搜（或单轮），2及以后为细搜第stage-1轮
    void progressUpdated(int percent);
    void candidateReady(const OptimizeCandidate &candidate);
    // 寻优结束（stopped=true 表示被手动停止）；
    // accepted>0 时 best 为已评估候选中材料成本最低的方案。
    void finished(bool stopped, const OptimizeCandidate &best, const OptimizationRunSummary &summary);
};

#endif // IOPTIMIZER_H
