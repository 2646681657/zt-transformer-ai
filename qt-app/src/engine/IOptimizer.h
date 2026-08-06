#ifndef IOPTIMIZER_H
#define IOPTIMIZER_H
// 优化器接口（异步执行多方案寻优，通过信号报告进度和结果）

#include <QObject>
#include <QVector>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "OptimizationResult.h"

struct OptimizationSettings {
    enum Method { Optimize, Exhaustive };
    Method method = Optimize;
    int threadCount = 4;
    enum CostModel { CuFe, CuFeOil };
    CostModel costModel = CuFeOil;
};

class IOptimizer : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IOptimizer() = default;

    // 启动优化计算（异步，结果通过 resultReady 信号逐个返回）
    virtual void start(const TransformerParams &params,
                       const StructureConfig &config,
                       const OptimizationSettings &settings) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;

signals:
    void progressUpdated(int percent);
    void resultReady(const OptimizationResult &result);
    void finished();
};

#endif // IOPTIMIZER_H
