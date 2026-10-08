#include "GridOptimizer.h"
#include "ElectromagneticEngine.h"
#include "SchemeConstraints.h"
#include "GridSearchSpace.h"
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QMetaObject>
#include <QElapsedTimer>
#include <QSet>
#include <algorithm>
#include <cmath>

// 后台工作对象：网格遍历 + 电磁计算（引擎无状态，线程内独立实例）
class GridOptimizer::Worker : public QObject {
    Q_OBJECT
public:
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
        QMutexLocker locker(&m_mutex);
        m_stopped = true;
        m_paused = false;
        m_cond.wakeAll();
    }

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

        // 两个阶段使用同一完整引擎、成本目标和冻结的约束。
        const auto evaluate = [&](const Point &point, OptimizationStageSummary &stage) {
            if (!waitIfPaused()) {
                stopped = true;
                return false;
            }
            visited.insert(GridSearchSpace::key(point));
            const CalcInput in = space.inputFor(point);
            auto &gradeSummary = summary.steelSummaries[space.steelGradeFor(point)];
            if (!m_base.isRoundHighVoltageWire() && in.isRoundHighVoltageWire()) {
                ++stage.wireFormRejected;
                ++gradeSummary.wireFormRejected;
                ++summary.rejectionReasons[QStringLiteral("扁线宽=厚（禁止自动切换圆线）")];
                emit progressUpdated(stage.processedCount() * 100 / stage.planned);
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
                emit candidateReady(c);
                ++stage.accepted;
                ++gradeSummary.accepted;
                const double cost = optimizationMaterialCost(r);
                if (!gradeSummary.hasBest || cost < gradeSummary.bestCost) {
                    gradeSummary.hasBest = true;
                    gradeSummary.bestCost = cost;
                }
                if (m_settings.hasRefinement()) {
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
            emit progressUpdated(stage.processedCount() * 100 / stage.planned);
            return true;
        };

        emit stageChanged(1, summary.coarse.planned);
        for (int index = 0; index < summary.coarse.planned; ++index) {
            if (!evaluate(space.coarsePoint(index), summary.coarse)) break;
        }
        // 阶段交界处暂停仍归入粗搜耗时，不只计入总耗时。
        if (m_settings.hasRefinement() && !stopped && !waitIfPaused())
            stopped = true;
        summary.coarse.elapsed_ms = timer.elapsed();
        summary.coarseCompleted = summary.coarse.processedCount() == summary.coarse.planned;
        summary.coarseHasBest = haveBest;
        if (haveBest) summary.coarseBestCost = optimizationMaterialCost(best.result);

        if (m_settings.hasRefinement()) {
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
                    // 生成期间中心快照不变；评估新候选后才更新下一轮中心。
                    for (const auto &seed : seeds) {
                        for (const auto &point : space.neighbors(seed.point, round)) {
                            ++stage.generated;
                            const QString key = GridSearchSpace::key(point);
                            if (visited.contains(key)) {
                                ++stage.duplicateSkipped;
                            } else {
                                visited.insert(key);
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
                            if (!evaluate(point, stage)) break;
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
        summary.elapsed_ms = timer.elapsed();
        summary.hasBest = haveBest;
        if (haveBest) summary.bestBoundaryHits = space.boundaryHits(bestPoint);
        emit workFinished(stopped, best, summary);
    }

signals:
    void stageChanged(int stage, int planned);
    void progressUpdated(int percent);
    void candidateReady(const OptimizeCandidate &candidate);
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
    TransformerParams m_params;    // 性能标准值（约束过滤使用）
    CalcInput m_base;              // 寻优基准设计变量
    OptimizationSettings m_settings;   // 网格范围/步长（可配置）
};

GridOptimizer::GridOptimizer(QObject *parent)
    : IOptimizer(parent)
{
    // 跨线程 queued connection 传递自定义类型需注册
    qRegisterMetaType<OptimizeCandidate>("OptimizeCandidate");
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
