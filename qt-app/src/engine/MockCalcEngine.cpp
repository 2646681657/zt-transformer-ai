#include "MockCalcEngine.h"

PrintOutputData MockCalcEngine::calculate(const TransformerParams &,
                                          const StructureConfig &)
{
    return PrintOutputData::createDefault();
}
