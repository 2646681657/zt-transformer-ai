#ifndef MOCKCALCENGINE_H
#define MOCKCALCENGINE_H
// Mock计算引擎（返回固定演示数据，用于UI开发阶段）

#include "ICalcEngine.h"

class MockCalcEngine : public ICalcEngine {
public:
    PrintOutputData calculate(const TransformerParams &params,
                              const StructureConfig &config) override;
};

#endif // MOCKCALCENGINE_H
