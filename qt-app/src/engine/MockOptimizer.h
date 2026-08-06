#ifndef MOCKOPTIMIZER_H
#define MOCKOPTIMIZER_H
// Mock优化器（模拟异步寻优过程，生成演示方案数据）

#include "IOptimizer.h"

class MockOptimizer : public IOptimizer {
    Q_OBJECT
public:
    explicit MockOptimizer(QObject *parent = nullptr);

    void start(const TransformerParams &params,
               const StructureConfig &config,
               const OptimizationSettings &settings) override;
    void pause() override {}
    void resume() override {}
    void stop() override {}

private:
    QVector<OptimizationResult> generateMockResults();
};

#endif // MOCKOPTIMIZER_H
