#include "GridOptimizer.h"
#include "ElectromagneticEngine.h"
#include "SchemeConstraints.h"
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QMetaObject>

namespace {

// 网格步进与邻域的默认值（OptimizationSettings 缺省即此，保持向后一致）：
// 直径 ±10mm(步进5) × 直线段 ±5mm(步进5) × 低压匝数 ±1 × 高压每层匝数 ±1 = 135 组合
constexpr double kDiaStep = 5.0;
constexpr int kDiaRange = 2;
constexpr double kStraightStep = 5.0;
constexpr int kStraightRange = 1;
constexpr int kLvTurnsRange = 1;
constexpr int kHvTplRange = 1;

} // namespace

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
        // 网格范围/步长取自设置（对话框可配，默认值与历史行为一致）
        const double diaStep = m_settings.diaStep_mm > 0 ? m_settings.diaStep_mm : kDiaStep;
        const int diaRange = m_settings.diaRange >= 0 ? m_settings.diaRange : kDiaRange;
        const double straightStep = m_settings.straightStep_mm > 0
                                        ? m_settings.straightStep_mm : kStraightStep;
        const int straightRange = m_settings.straightRange >= 0
                                      ? m_settings.straightRange : kStraightRange;
        const int lvTurnsRange = m_settings.lvTurnsRange >= 0
                                     ? m_settings.lvTurnsRange : kLvTurnsRange;
        const int hvTplRange = m_settings.hvTplRange >= 0 ? m_settings.hvTplRange : kHvTplRange;

        const int total = (2 * diaRange + 1) * (2 * straightRange + 1)
                          * (2 * lvTurnsRange + 1) * (2 * hvTplRange + 1);
        ElectromagneticEngine engine;
        int done = 0;
        int valid = 0;
        OptimizeCandidate best;
        bool haveBest = false;
        bool stopped = false;

        for (int di = -diaRange; di <= diaRange && !stopped; ++di) {
            for (int si = -straightRange; si <= straightRange && !stopped; ++si) {
                for (int li = -lvTurnsRange; li <= lvTurnsRange && !stopped; ++li) {
                    for (int hi = -hvTplRange; hi <= hvTplRange && !stopped; ++hi) {
                        if (!waitIfPaused()) {
                            stopped = true;
                            break;
                        }
                        CalcInput in = m_base;
                        in.coreDiameter_mm += di * diaStep;
                        in.coreStraight_mm += si * straightStep;
                        in.lvTurns += li;
                        in.hvTurnsPerLayer += hi;

                        CalcResult r;
                        if (engine.calcElectromagnetic(in, r) && r.valid
                                && checkSchemeConstraints(m_params, r).passed) {
                            OptimizeCandidate c;
                            c.input = in;
                            c.result = r;
                            c.scheme = makeScheme(0, in, r);  // 序号由接收端按入库顺序编排
                            emit candidateReady(c);
                            ++valid;
                            if (!haveBest
                                    || r.cost.materialCost < best.result.cost.materialCost) {
                                best = c;
                                haveBest = true;
                            }
                        }
                        ++done;
                        emit progressUpdated(done * 100 / total);
                    }
                }
            }
        }
        emit workFinished(stopped, best, total, valid);
    }

signals:
    void progressUpdated(int percent);
    void candidateReady(const OptimizeCandidate &candidate);
    void workFinished(bool stopped, const OptimizeCandidate &best,
                      int total, int valid);

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
    connect(m_worker, &Worker::candidateReady,
            this, &GridOptimizer::candidateReady);
    connect(m_worker, &Worker::workFinished, this,
            [this](bool stopped, const OptimizeCandidate &best, int total, int valid) {
                emit finished(stopped, best, total, valid);
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
