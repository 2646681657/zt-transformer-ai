#ifndef ICALCENGINE_H
#define ICALCENGINE_H

#include "TransformerParams.h"
#include "StructureConfig.h"
#include "PrintOutputData.h"

class ICalcEngine {
public:
    virtual ~ICalcEngine() = default;
    virtual PrintOutputData calculate(const TransformerParams &params,
                                      const StructureConfig &config) = 0;
};

#endif // ICALCENGINE_H
