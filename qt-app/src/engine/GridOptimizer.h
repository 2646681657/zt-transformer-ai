#ifndef GRIDOPTIMIZER_H
#define GRIDOPTIMIZER_H
// 网格寻优器：围绕基准设计变量（CalcInput）的小邻域离散步进遍历，
// 每个组合调用电磁计算引擎得到候选方案；后台线程执行，支持暂停/恢复/停止

#include "IOptimizer.h"
#include <QPointer>

class QThread;

class GridOptimizer : public IOptimizer {
    Q_OBJECT
public:
    explicit GridOptimizer(QObject *parent = nullptr);
    ~GridOptimizer() override;

    void start(const TransformerParams &params,
               const StructureConfig &config,
               const CalcInput &baseInput,
               const OptimizationSettings &settings) override;
    void pause() override;
    void resume() override;
    void stop() override;

private:
    class Worker;
    QPointer<Worker> m_worker;    // 后台工作对象（线程结束后自动销毁并置空）
    QPointer<QThread> m_thread;   // 当前寻优线程（结束后自动销毁并置空）
};

#endif // GRIDOPTIMIZER_H
