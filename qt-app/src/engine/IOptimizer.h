#ifndef IOPTIMIZER_H
#define IOPTIMIZER_H
// 优化器接口（异步执行多方案寻优，通过信号报告进度和候选方案）

#include <QObject>
#include <QVector>
#include <QMetaType>
#include "TransformerParams.h"
#include "StructureConfig.h"
#include "CalcInput.h"
#include "CalcResult.h"
#include "OptimizationResult.h"

struct OptimizationSettings {
    enum Method { Optimize, Exhaustive };
    Method method = Optimize;
    int threadCount = 4;
    enum CostModel { CuFe, CuFeOil };
    CostModel costModel = CuFeOil;
    // 网格搜索范围（围绕基准设计变量的 ±N 步）与步长
    double diaStep_mm = 5.0;      // 铁芯直径步进 mm
    int diaRange = 2;             // 直径 ±2 步（5 档）
    double straightStep_mm = 5.0; // 直线段长步进 mm
    int straightRange = 1;        // 直线段 ±1 步（3 档）
    int lvTurnsRange = 1;         // 低压匝数 ±1（3 档）
    int hvTplRange = 1;           // 高压每层匝数 ±1（3 档）
};

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
    // total=评估组合数，valid=通过约束校验入库的方案数（其余被剔除）；
    // valid>0 时 best 为其中材料成本最低的候选方案
    void finished(bool stopped, const OptimizeCandidate &best, int total, int valid);
};

#endif // IOPTIMIZER_H
