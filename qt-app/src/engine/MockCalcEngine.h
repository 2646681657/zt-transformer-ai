#ifndef MOCKCALCENGINE_H
#define MOCKCALCENGINE_H

#include "ICalcEngine.h"

class MockCalcEngine : public ICalcEngine {
public:
    PrintOutputData calculate(const TransformerParams &params,
                              const StructureConfig &config) override;
};

#endif // MOCKCALCENGINE_H
