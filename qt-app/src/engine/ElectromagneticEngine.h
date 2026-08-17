#ifndef ELECTROMAGNETICENGINE_H
#define ELECTROMAGNETICENGINE_H
// 电磁计算引擎：SB20 计算单公式链路的 C++ 翻译
// 链路：额定值 → 匝电压 → 铁芯叠积 → 磁密/空载损耗 → 绕组尺寸 →
//       导线长/电阻/负载损耗 → 阻抗电压 → 温升 → 重量/成本

#include "ICalcEngine.h"

class ElectromagneticEngine : public ICalcEngine {
public:
    // 旧接口（打印输出）：暂返回未接线提示，待 GUI 打印页接入电磁结果后填充
    PrintOutputData calculate(const TransformerParams &params,
                              const StructureConfig &config) override;

    // 电磁计算全链路：CalcInput（默认即 SB20-M-630-10）→ CalcResult
    bool calcElectromagnetic(const CalcInput &input, CalcResult &result) override;

    // 对拍自检：用 SB20-M-630-10 计算单缓存值验证全链路，返回逐项对照文本
    static QString selfTestReport();
};

#endif  // ELECTROMAGNETICENGINE_H
