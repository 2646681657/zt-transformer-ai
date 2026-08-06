#include "MockOptimizer.h"
#include <QTimer>

MockOptimizer::MockOptimizer(QObject *parent)
    : IOptimizer(parent) {}

// 模拟异步优化：延时100ms后一次性发射所有模拟方案结果
void MockOptimizer::start(const TransformerParams &,
                          const StructureConfig &,
                          const OptimizationSettings &)
{
    auto results = generateMockResults();
    QTimer::singleShot(100, this, [this, results]() {
        for (int i = 0; i < results.size(); ++i) {
            emit resultReady(results[i]);
            emit progressUpdated((i + 1) * 100 / results.size());
        }
        emit finished();
    });
}

QVector<OptimizationResult> MockOptimizer::generateMockResults()
{
    QVector<OptimizationResult> results;
    double baseCost = 18000.0;
    for (int i = 1; i <= 31; ++i) {
        OptimizationResult r;
        r.schemeIdx = i;
        r.costCuFeOil = baseCost + i * 120.5;
        r.costCuFe = r.costCuFeOil * 0.85;
        r.coreD = 230.0 + i * 2.0;
        r.coreL = 1.45 + i * 0.01;
        r.lvTurns = 18 + (i % 5);
        r.lvRuleT = 1.8 + (i % 3) * 0.1;
        r.lvRuleW = 9.0 + (i % 4) * 0.5;
        r.hvRuleT = 1.0 + (i % 3) * 0.05;
        r.hvRuleW = 4.5 + (i % 4) * 0.2;
        r.hvLayers = 10 + (i % 4);
        r.lvOilDucts = 3 + (i % 3);
        r.hvOilDucts = 4 + (i % 3);
        r.lvToYoke = 15.0 + (i % 5);
        r.mainDuct = 12.0 + (i % 3);
        r.lvHalfOilDucts = 1 + (i % 2);
        r.hvHalfOilDucts = 2 + (i % 2);
        r.lvHalfDist = 8.0 + (i % 4) * 0.5;
        results.append(r);
    }
    return results;
}
