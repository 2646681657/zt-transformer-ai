#ifndef ICALCENGINE_H
#define ICALCENGINE_H
// 计算引擎接口（根据参数和结构配置执行电磁计算）

#include "TransformerParams.h"
#include "StructureConfig.h"
#include "PrintOutputData.h"
#include "CalcInput.h"
#include "CalcResult.h"

class ICalcEngine {
public:
    virtual ~ICalcEngine() = default;
    // 执行一次完整计算，返回可打印的结果数据
    virtual PrintOutputData calculate(const TransformerParams &params,
                                      const StructureConfig &config) = 0;
    // 电磁计算全链路（CalcInput 设计变量 → CalcResult 结果）
    // 默认实现返回失败；由支持电磁计算的引擎覆写
    virtual bool calcElectromagnetic(const CalcInput &input, CalcResult &result)
    {
        Q_UNUSED(input);
        Q_UNUSED(result);
        return false;
    }
};

#endif // ICALCENGINE_H
