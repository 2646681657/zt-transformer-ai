#include "GridOptimizer.h"
#include "ElectromagneticEngine.h"
#include "SchemeConstraints.h"
#include "GridSearchSpace.h"
#include "SearchCenterPool.h"
#include "SearchCoveragePlanner.h"
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QMetaObject>
#include <QElapsedTimer>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <memory>

// 后台工作对象：网格遍历 + 电磁计算（引擎无状态，线程内独立实例）
class GridOptimizer::Worker : public QObject {
    Q_OBJECT
public:
    struct BatchDelivery {
        QMutex mutex;
        QWaitCondition condition;
        bool pending = false;
        bool stopped = false;
    };
    explicit Worker(const TransformerParams &params, const CalcInput &base,
                    const OptimizationSettings &settings)
        : m_params(params), m_base(base), m_settings(settings) {}

    // 控制接口（互斥锁保护，可从主线程直接调用）
    void pause()
    {
        QMutexLocker locker(&m_mutex);
        m_paused = true;
    }

    void resume()
    {
        QMutexLocker locker(&m_mutex);
        m_paused = false;
        m_cond.wakeAll();
    }

    void stop()
    {
        {
            QMutexLocker locker(&m_mutex);
            m_stopped = true;
            m_paused = false;
            m_cond.wakeAll();
        }
        QMutexLocker locker(&m_delivery->mutex);
        m_delivery->stopped = true;
        m_delivery->condition.wakeAll();
    }

    // Called in the GUI thread after forwarding the one outstanding snapshot.
    static void acknowledgeCandidates(const std::shared_ptr<BatchDelivery> &delivery) {
        QMutexLocker locker(&delivery->mutex);
        delivery->pending = false;
        delivery->condition.wakeAll();
    }
    std::shared_ptr<BatchDelivery> deliveryState() const { return m_delivery; }

public slots:
    void doWork()
    {
        OptimizationRunSummary summary;
        summary.planned = m_settings.plannedCount();
        summary.coarse.planned = summary.planned;
        summary.error = m_settings.validationError(m_base);
        // 未校核项仅依赖冻结的订单标准，与候选结果无关。
        summary.skippedChecks = checkSchemeConstraints(m_params, CalcResult{}).skippedChecks;
        if (!summary.error.isEmpty()) {
            emit workFinished(false, OptimizeCandidate{}, summary);
            return;
        }
        QElapsedTimer timer;
        timer.start();
        const GridSearchSpace space(m_base, m_settings);
        SearchCenterPool centerPool(space);
        SearchCoveragePlanner planner(space, m_settings);
        summary.coverageDetails.append(m_settings.craftConstraints.description());
        summary.boundarySearchDimensions = space.boundarySearchDimensions();
        const QStringList grades = m_settings.searchSteelGrade ? m_settings.steelGrades
            : QStringList{m_base.steelGrade.trimmed()};
        for (const auto &grade : grades)
            summary.steelSummaries[grade].planned = summary.coarse.planned / int(grades.size());
        using Point = GridSearchSpace::Point;
        struct Seed { Point point; double cost; };
        QVector<Seed> seeds;
        QSet<QString> visited;
        ElectromagneticEngine engine;
        OptimizeCandidate best;
        Point bestPoint{};
        bool haveBest = false;
        bool stopped = false;
        bool budgetExhausted = false;
        int processed = 0;
        int lastProgress = -1;
        int phaseNumber = 1;
        int selectionPhase = 0;
        struct Retained { OptimizeCandidate candidate; double cost; qint64 order; };
        QVector<Retained> retained;
        const auto cheaper = [](const Retained &a, const Retained &b) {
            return a.cost < b.cost || (a.cost == b.cost && a.order < b.order);
        };
        const auto reportProgress = [&](const OptimizationStageSummary &stage) {
            const int percent = m_settings.enhancedCoverage ? processed * 100 / m_settings.combinationBudget
                : (stage.planned > 0 ? stage.processedCount() * 100 / stage.planned : 0);
            if (!m_settings.enhancedCoverage || percent != lastProgress) {
                emit progressUpdated(percent);
                lastProgress = percent;
            }
        };
        const auto publishCandidates = [&]() {
            if (!m_settings.enhancedCoverage) return;
            QVector<Retained> ordered = retained;
            std::sort(ordered.begin(), ordered.end(), cheaper);
            QVector<OptimizeCandidate> candidates;
            candidates.reserve(ordered.size());
            for (const auto &item : ordered) candidates.append(item.candidate);
            {
                QMutexLocker locker(&m_delivery->mutex);
                m_delivery->pending = true;
            }
            emit candidatesReady(candidates);
            // Stop wakes this wait, allowing main-thread destructor->thread.wait() to finish.
            QMutexLocker locker(&m_delivery->mutex);
            while (m_delivery->pending && !m_delivery->stopped)
                m_delivery->condition.wait(&m_delivery->mutex);
            if (m_delivery->stopped) stopped = true;
        };

        // 两个阶段使用同一完整引擎、成本目标和冻结的约束。
        const auto evaluate = [&](const Point &point, OptimizationStageSummary &stage) {
            if (!waitIfPaused()) {
                stopped = true;
                return false;
            }
            if (processed >= m_settings.combinationBudget) {
                budgetExhausted = true;
                return false;
            }
            visited.insert(GridSearchSpace::key(point));
            ++processed;
            const CalcInput in = space.inputFor(point);
            planner.recordProcessed(point, in);
            auto &gradeSummary = summary.steelSummaries[space.steelGradeFor(point)];
            if (!m_base.isRoundHighVoltageWire() && in.isRoundHighVoltageWire()) {
                ++stage.wireFormRejected;
                ++gradeSummary.wireFormRejected;
                ++summary.rejectionReasons[QStringLiteral("扁线宽=厚（禁止自动切换圆线）")];
                reportProgress(stage);
                return true;
            }
            if (!in.craftConstraints.rejectionReason(in.mainDuctWidth_mm).isEmpty()) {
                ++stage.craftRejected;
                ++gradeSummary.craftRejected;
                ++summary.rejectionReasons[QStringLiteral("企业主油道最小宽度")];
                reportProgress(stage);
                return true;
            }

            CalcResult r;
            ++stage.evaluated;
            ++gradeSummary.evaluated;
            const bool calculated = engine.calcElectromagnetic(in, r) && r.valid;
            const auto constraints = calculated ? checkSchemeConstraints(m_params, r)
                                                : SchemeConstraintsResult{};
            if (!calculated || !std::isfinite(optimizationMaterialCost(r))
                    || optimizationMaterialCost(r) < 0.0) {
                ++stage.invalid;
                ++gradeSummary.invalid;
                ++summary.rejectionReasons[QStringLiteral("计算失败或材料成本无效")];
                QString reason = calculated ? QStringLiteral("材料成本非有限值或为负数") : r.error.trimmed();
                if (reason.isEmpty()) reason = QStringLiteral("引擎未返回有效结果");
                reason = reason.left(240);
                if (!summary.calculationErrors.contains(reason) && summary.calculationErrors.size() >= 20)
                    reason = QStringLiteral("其他计算失败原因");
                ++summary.calculationErrors[reason];
            } else if (!constraints.passed) {
                ++stage.constraintRejected;
                ++gradeSummary.constraintRejected;
                for (const auto &name : constraints.failedChecks)
                    ++summary.rejectionReasons[name];
            } else {
                OptimizeCandidate c;
                c.input = in;
                c.result = r;
                c.scheme = makeScheme(0, in, r);  // 序号由接收端按入库顺序编排
                if (!m_settings.enhancedCoverage) emit candidateReady(c);
                ++stage.accepted;
                ++gradeSummary.accepted;
                const double cost = optimizationMaterialCost(r);
                if (m_settings.enhancedCoverage) {
                    centerPool.add(point, cost, processed);
                    Retained item{c, cost, processed};
                    if (retained.size() < 5000) {
                        retained.append(std::move(item));
                        std::push_heap(retained.begin(), retained.end(), cheaper);
                    } else if (cheaper(item, retained.first())) {
                        std::pop_heap(retained.begin(), retained.end(), cheaper);
                        retained.last() = std::move(item);
                        std::push_heap(retained.begin(), retained.end(), cheaper);
                    }
                }
                if (!gradeSummary.hasBest || cost < gradeSummary.bestCost) {
                    gradeSummary.hasBest = true;
                    gradeSummary.bestCost = cost;
                }
                if (m_settings.hasRefinement() && !m_settings.enhancedCoverage) {
                    seeds.append({point, optimizationMaterialCost(r)});
                    std::stable_sort(seeds.begin(), seeds.end(), [](const Seed &a, const Seed &b) {
                        return a.cost < b.cost;
                    });
                    if (seeds.size() > OptimizationSettings::fineSeedLimit) seeds.removeLast();
                }
                if (!haveBest || optimizationMaterialCost(r) < optimizationMaterialCost(best.result)) {
                    best = c;
                    bestPoint = point;
                    summary.bestPerformanceMargins = schemeConstraintMarginText(constraints);
                    haveBest = true;
                }
            }
            reportProgress(stage);
            return true;
        };

        emit stageChanged(1, summary.coarse.planned);
        for (int index = 0; index < summary.coarse.planned; ++index) {
            if (!evaluate(space.coarsePoint(index), summary.coarse)) break;
        }
        // 阶段交界处暂停仍归入粗搜耗时，不只计入总耗时。
        if (m_settings.hasRefinement() && !stopped && !waitIfPaused())
            stopped = true;
        publishCandidates();
        summary.coarse.elapsed_ms = timer.elapsed();
        summary.coarseCompleted = summary.coarse.processedCount() == summary.coarse.planned;
        summary.coarseHasBest = haveBest;
        if (haveBest) summary.coarseBestCost = optimizationMaterialCost(best.result);

        if (m_settings.enhancedCoverage) {
            // Each phase plans only unique points fitting the remaining budget. Unscheduled
            // regions are disclosed separately, never mislabeled as a completed full space.
            const auto runPhase = [&](const QVector<SearchCenterPool::Center> &centers,
                                      QVector<SearchCoveragePlanner::Region> regions,
                                      int round, const QString &label, bool expansion) {
                QElapsedTimer stageTimer;
                stageTimer.start();
                OptimizationRefinementSummary stage;
                stage.round = round;
                stage.phaseLabel = label;
                stage.seedCount = int(centers.size());
                stage.bestCostBefore = haveBest ? optimizationMaterialCost(best.result) : 0.0;
                summary.centerDetails.append(centerPool.details(label, centers));
                QSet<QString> scheduled;
                QVector<Point> points;
                const int remaining = m_settings.combinationBudget - processed;
                bool scheduleLimited = false;
                SearchCoveragePlanner::visitInterleaved(std::move(regions), [&](const Point &point) {
                    if (!waitIfPaused()) { stopped = true; return false; }
                    const QString key = GridSearchSpace::key(point);
                    if (visited.contains(key) || scheduled.contains(key)) {
                        ++stage.generated;
                        ++stage.duplicateSkipped;
                        return true;
                    }
                    if (points.size() >= remaining) { scheduleLimited = true; return false; }
                    ++stage.generated;
                    scheduled.insert(key);
                    points.append(point);
                    ++summary.steelSummaries[space.steelGradeFor(point)].planned;
                    return true;
                });
                stage.planned = int(points.size());
                const auto beforeGrades = summary.steelSummaries;
                if (!points.isEmpty()) {
                    if (!expansion) summary.fineStarted = true;
                    emit stageChanged(++phaseNumber, stage.planned);
                    for (const auto &point : points) {
                        if (stopped || !evaluate(point, stage)) break;
                    }
                }
                if (!stopped && !waitIfPaused()) stopped = true;
                publishCandidates();
                stage.elapsed_ms = stageTimer.elapsed(); // Include snapshot acknowledgement and pause time.
                stage.completed = stage.planned > 0 && !stopped && !scheduleLimited
                    && stage.processedCount() == stage.planned;
                stage.bestCostAfter = haveBest ? optimizationMaterialCost(best.result) : 0.0;
                stage.improvement_pct = stage.bestCostBefore > 0.0
                    ? (stage.bestCostBefore - stage.bestCostAfter) / stage.bestCostBefore * 100.0 : 0.0;
                summary.duplicateSkipped += stage.duplicateSkipped;
                if (expansion) summary.expansions.append(stage);
                else {
                    summary.refinements.append(stage);
                    summary.fine.accumulate(stage);
                    summary.fineSeedCount += stage.seedCount;
                    summary.fineGenerated += stage.generated;
                }
                for (const auto &grade : grades) {
                    const auto &after = summary.steelSummaries[grade];
                    const auto before = beforeGrades.value(grade);
                    summary.centerDetails.append(QStringLiteral("%1 · %2：实际处理%3，引擎评估%4，可行%5（中心选择不等于已处理）。")
                        .arg(label, grade).arg(after.processedCount() - before.processedCount())
                        .arg(after.evaluated - before.evaluated).arg(after.accepted - before.accepted));
                }
                // Include evictions caused by this phase, not just its pre-evaluation pool.
                summary.centerDetails.append(centerPool.details(label + QStringLiteral("结束池统计"), centers));
                summary.coverageDetails.append(QStringLiteral("%1：计划%2，实际处理%3，未处理%4，跨阶段/区域重复%5；%6。")
                    .arg(label).arg(stage.planned).arg(stage.processedCount()).arg(stage.planned - stage.processedCount())
                    .arg(stage.duplicateSkipped).arg(scheduleLimited
                        ? QStringLiteral("剩余预算限制生成，其他区域未规划，不能称全部搜索完成")
                        : QStringLiteral("仅针对本阶段生成区域")));
                if (scheduleLimited || processed >= m_settings.combinationBudget) budgetExhausted = true;
                return stage;
            };
            const auto localRefine = [&](int expansionRound, int rounds) {
                int stagnant = 0;
                for (int round = 1; round <= rounds && !stopped && !budgetExhausted; ++round) {
                    const QString label = expansionRound == 0 ? QStringLiteral("初始区域细搜第%1轮").arg(round)
                        : QStringLiteral("扩展第%1轮后区域细搜第%2轮").arg(expansionRound).arg(round);
                    const auto centers = centerPool.select(m_settings.centerLimit, selectionPhase++);
                    const auto stage = runPhase(centers, planner.localRegions(centers, round), round, label, false);
                    if (!stage.completed || stage.planned == 0) {
                        summary.coverageDetails.append(label + QStringLiteral("没有完整新增局部组合；仍可继续允许的扩展方向。"));
                        break;
                    }
                    stagnant = stage.improvement_pct < OptimizationSettings::improvementThreshold_pct ? stagnant + 1 : 0;
                    if (stagnant >= OptimizationSettings::stagnationRoundLimit) {
                        summary.coverageDetails.append(label + QStringLiteral("达到局部低改善停止条件；不会取消后续扩展。"));
                        break;
                    }
                }
            };
            if (processed >= m_settings.combinationBudget && (m_settings.hasRefinement() || m_settings.expandCoverage))
                budgetExhausted = true;
            if (!stopped && !budgetExhausted && haveBest) localRefine(0, m_settings.fineRoundCount());
            bool noExpansion = false;
            int expansionsDone = 0;
            if (m_settings.expandCoverage && haveBest) {
                for (int round = 1; round <= m_settings.maxExpansionRounds && !stopped && !budgetExhausted; ++round) {
                    const auto centers = centerPool.select(m_settings.centerLimit, selectionPhase++);
                    const auto regions = planner.expansionRegions(centers, round - 1);
                    const auto stage = runPhase(centers, regions, round, QStringLiteral("受控扩展第%1轮").arg(round), true);
                    ++expansionsDone;
                    // Refine newly explored regions between consecutive expansion rounds.
                    if (!stopped && !budgetExhausted && stage.processedCount() > 0)
                        localRefine(round, qMax(1, m_settings.fineRoundCount()));
                    if (stage.planned == 0) {
                        // No point from this rotation does not rule out another grade's frontier.
                        noExpansion = true;
                        summary.coverageDetails.append(QStringLiteral("扩展第%1轮所选中心没有新增允许触边点；剩余轮次继续轮转牌号。").arg(round));
                    } else noExpansion = false;
                }
            }
            summary.stopReason = stopped ? QStringLiteral("人工停止；仅交付已处理范围内的保留候选。")
                : budgetExhausted ? QStringLiteral("总处理预算耗尽；存在未处理或未规划区域，不代表全部空间已搜索。")
                : !haveBest ? QStringLiteral("初始粗搜无可行方案，未进入区域细搜/扩展；不放宽性能和工艺约束。")
                : m_settings.expandCoverage ? (noExpansion
                    ? QStringLiteral("硬边界/所选中心无新增触边点且扩展轮数耗尽（已尝试%1轮）；不证明全局覆盖。").arg(expansionsDone)
                    : QStringLiteral("已达到最大扩展轮数%1轮；各轮后区域细化完成或局部停止，不证明全局最优。").arg(expansionsDone))
                : m_settings.hasRefinement() ? QStringLiteral("区域细化达到轮数、无新增点或局部低改善条件；未启用扩展，不证明全局最优。")
                : QStringLiteral("初始单轮网格遍历完成；未启用细搜/扩展。");
            if (!summary.fineStarted && m_settings.hasRefinement()) summary.fineSkipReason = summary.stopReason;
        } else if (m_settings.hasRefinement()) {
            if (stopped) {
                summary.stopReason = QStringLiteral("粗搜阶段人工停止，未进入细搜。");
            } else if (seeds.isEmpty()) {
                summary.stopReason = QStringLiteral("粗搜没有可行方案，未进入细搜；订单约束不放宽。");
            } else {
                int stagnantRounds = 0;
                for (int round = 1; round <= m_settings.fineRoundCount(); ++round) {
                    QElapsedTimer fineTimer;
                    fineTimer.start();
                    OptimizationRefinementSummary stage;
                    stage.round = round;
                    stage.seedCount = int(seeds.size());
                    stage.bestCostBefore = optimizationMaterialCost(best.result);
                    QVector<Point> finePoints;
                    QSet<QString> scheduled;
                    // 生成期间中心快照不变；评估新候选后才更新下一轮中心。
                    for (const auto &seed : seeds) {
                        if (stopped) break;
                        const auto continueGeneration = [&]() {
                            if (waitIfPaused()) return true;
                            stopped = true;
                            return false;
                        };
                        for (const auto &point : space.neighbors(seed.point, round, continueGeneration)) {
                            ++stage.generated;
                            const QString key = GridSearchSpace::key(point);
                            if (visited.contains(key) || scheduled.contains(key)) {
                                ++stage.duplicateSkipped;
                            } else {
                                scheduled.insert(key);
                                finePoints.append(point);
                                ++summary.steelSummaries[space.steelGradeFor(point)].planned;
                            }
                        }
                    }
                    stage.planned = int(finePoints.size());
                    if (!finePoints.isEmpty()) {
                        summary.fineStarted = true;
                        emit stageChanged(round + 1, stage.planned);
                        emit progressUpdated(0);
                        for (const auto &point : finePoints) {
                            if (stopped || !evaluate(point, stage)) break;
                        }
                    }
                    // 暂停/停止的交界等待归入当前轮；不把未完成轮计入低改善。
                    if (!stopped && !waitIfPaused()) stopped = true;
                    stage.elapsed_ms = fineTimer.elapsed();
                    stage.completed = stage.planned > 0 && stage.processedCount() == stage.planned;
                    stage.bestCostAfter = optimizationMaterialCost(best.result);
                    stage.improvement_pct = stage.bestCostBefore > 0.0
                        ? (stage.bestCostBefore - stage.bestCostAfter) / stage.bestCostBefore * 100.0 : 0.0;
                    summary.refinements.append(stage);
                    summary.fine.accumulate(stage);
                    summary.fineSeedCount += stage.seedCount;
                    summary.fineGenerated += stage.generated;
                    summary.duplicateSkipped += stage.duplicateSkipped;

                    if (stopped) {
                        summary.stopReason = QStringLiteral("细搜第%1轮人工停止；仅保留已处理范围。").arg(round);
                        break;
                    }
                    if (finePoints.isEmpty()) {
                        summary.stopReason = QStringLiteral("细搜第%1轮没有新增组合；整数和圆线不插值，跨轮重复不重算。").arg(round);
                        break;
                    }
                    stagnantRounds = stage.improvement_pct < OptimizationSettings::improvementThreshold_pct
                        ? stagnantRounds + 1 : 0;
                    if (m_settings.method == OptimizationSettings::MultiRound
                            && stagnantRounds >= OptimizationSettings::stagnationRoundLimit) {
                        summary.stopReason = QStringLiteral("连续%1轮成本改善不足%2%，提前停止（局部停止条件，不代表全局最优）。")
                            .arg(OptimizationSettings::stagnationRoundLimit).arg(OptimizationSettings::improvementThreshold_pct);
                        break;
                    }
                    if (round == m_settings.fineRoundCount())
                        summary.stopReason = QStringLiteral("已达到最大细搜轮数%1轮。").arg(round);
                }
            }
            if (!summary.fineStarted) summary.fineSkipReason = summary.stopReason;
        } else {
            summary.stopReason = stopped ? QStringLiteral("单轮网格人工停止。") : QStringLiteral("单轮网格遍历完成。");
        }
        summary.planned = summary.coarse.planned + summary.fine.planned;
        summary.evaluated = summary.coarse.evaluated + summary.fine.evaluated;
        summary.accepted = summary.coarse.accepted + summary.fine.accepted;
        summary.invalid = summary.coarse.invalid + summary.fine.invalid;
        summary.constraintRejected = summary.coarse.constraintRejected + summary.fine.constraintRejected;
        summary.wireFormRejected = summary.coarse.wireFormRejected + summary.fine.wireFormRejected;
        summary.craftRejected = summary.coarse.craftRejected + summary.fine.craftRejected;
        for (const auto &expansion : summary.expansions)
            summary.accumulate(expansion);
        summary.retained = m_settings.enhancedCoverage ? int(retained.size()) : summary.accepted;
        summary.omitted = summary.accepted - summary.retained;
        summary.coverageDetails.append(planner.coverageDetails());
        summary.coverageDetails.append(QStringLiteral("唯一处理预算%1/%2；引擎评估%3；实际可行%4，保留%5，未保留%6；预算包含线型和企业工艺预检淘汰。")
            .arg(processed).arg(m_settings.combinationBudget).arg(summary.evaluated)
            .arg(summary.accepted).arg(summary.retained).arg(summary.omitted));
        if (m_settings.enhancedCoverage)
            summary.centerDetails.append(centerPool.details(QStringLiteral("最终中心池"), {}, false));
        summary.elapsed_ms = timer.elapsed();
        summary.hasBest = haveBest;
        if (haveBest) summary.bestBoundaryHits = space.boundaryHits(bestPoint);
        emit workFinished(stopped, best, summary);
    }

signals:
    void stageChanged(int stage, int planned);
    void progressUpdated(int percent);
    void candidateReady(const OptimizeCandidate &candidate);
    void candidatesReady(const QVector<OptimizeCandidate> &candidates);
    void workFinished(bool stopped, const OptimizeCandidate &best,
                      const OptimizationRunSummary &summary);

private:
    // 暂停时阻塞等待；返回 false 表示已请求停止
    bool waitIfPaused()
    {
        QMutexLocker locker(&m_mutex);
        while (m_paused && !m_stopped) {
            m_cond.wait(&m_mutex);
        }
        return !m_stopped;
    }

    QMutex m_mutex;
    QWaitCondition m_cond;
    bool m_paused = false;
    bool m_stopped = false;
    std::shared_ptr<BatchDelivery> m_delivery = std::make_shared<BatchDelivery>();
    TransformerParams m_params;    // 性能标准值（约束过滤使用）
    CalcInput m_base;              // 寻优基准设计变量
    OptimizationSettings m_settings;   // 网格范围/步长（可配置）
};

GridOptimizer::GridOptimizer(QObject *parent)
    : IOptimizer(parent)
{
    // 跨线程 queued connection 传递自定义类型需注册
    qRegisterMetaType<OptimizeCandidate>("OptimizeCandidate");
    qRegisterMetaType<QVector<OptimizeCandidate>>("QVector<OptimizeCandidate>");
    qRegisterMetaType<OptimizationRunSummary>("OptimizationRunSummary");
}

GridOptimizer::~GridOptimizer()
{
    if (m_worker) {
        m_worker->stop();   // 加速 doWork 返回
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        // 线程已结束：worker 已随之销毁，线程对象手工回收
        // （pending deleteLater 事件随对象析构自动丢弃）
        delete m_thread.data();
    }
}

void GridOptimizer::start(const TransformerParams &params, const StructureConfig &,
                          const CalcInput &baseInput, const OptimizationSettings &settings)
{
    if (m_thread && m_thread->isRunning()) {
        return;   // 已在寻优中，不重复启动
    }
    m_worker = new Worker(params, baseInput, settings);
    QThread *thread = new QThread();
    m_worker->moveToThread(thread);

    // 线程结束后自动回收 worker 与线程对象（QPointer 随之置空）
    connect(thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(m_worker, &Worker::progressUpdated,
            this, &GridOptimizer::progressUpdated);
    connect(m_worker, &Worker::stageChanged, this, &GridOptimizer::stageChanged);
    connect(m_worker, &Worker::candidateReady,
            this, &GridOptimizer::candidateReady);
    // Shared acknowledgement state outlives a stopped/deleted worker; no dangling QObject call.
    const auto batchDelivery = m_worker->deliveryState();
    connect(m_worker, &Worker::candidatesReady, this,
            [this, batchDelivery](const QVector<OptimizeCandidate> &candidates) {
                emit candidatesReady(candidates);
                Worker::acknowledgeCandidates(batchDelivery);
            });
    connect(m_worker, &Worker::workFinished, this,
            [this](bool stopped, const OptimizeCandidate &best, const OptimizationRunSummary &summary) {
                emit finished(stopped, best, summary);
            });
    connect(m_worker, &Worker::workFinished, thread, &QThread::quit);

    m_thread = thread;
    thread->start();
    QMetaObject::invokeMethod(m_worker, "doWork", Qt::QueuedConnection);
}

void GridOptimizer::pause()
{
    if (m_worker && m_thread && m_thread->isRunning()) {
        m_worker->pause();
    }
}

void GridOptimizer::resume()
{
    if (m_worker && m_thread && m_thread->isRunning()) {
        m_worker->resume();
    }
}

void GridOptimizer::stop()
{
    if (m_worker && m_thread && m_thread->isRunning()) {
        m_worker->stop();
    }
}

#include "GridOptimizer.moc"
