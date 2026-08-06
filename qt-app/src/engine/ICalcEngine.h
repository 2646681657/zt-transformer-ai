#ifndef ICALCENGINE_H
#define ICALCENGINE_H
// 计算引擎接口（根据参数和结构配置执行电磁计算）

#include "TransformerParams.h"
#include "StructureConfig.h"
#include "PrintOutputData.h"

class ICalcEngine {
public:
    virtual ~ICalcEngine() = default;
    // 执行一次完整计算，返回可打印的结果数据
    virtual PrintOutputData calculate(const TransformerParams &params,
                                      const StructureConfig &config) = 0;
};

#endif // ICALCENGINE_H
