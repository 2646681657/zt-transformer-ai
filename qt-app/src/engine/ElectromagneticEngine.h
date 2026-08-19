#ifndef ELECTROMAGNETICENGINE_H
#define ELECTROMAGNETICENGINE_H
// 电磁计算引擎：SB20 计算单公式链路的 C++ 翻译
// 链路：额定值 → 匝电压 → 铁芯叠积 → 磁密/空载损耗 → 绕组尺寸 →
//       导线长/电阻/负载损耗 → 阻抗电压 → 温升 → 重量/成本

#include "ICalcEngine.h"

class ElectromagneticEngine : public ICalcEngine {
public:
    // 旧接口（打印输出）：以默认设计变量（SB20）计算并组行
    PrintOutputData calculate(const TransformerParams &params,
                              const StructureConfig &config) override;

    // 任意方案的计算结果 → 打印双栏行（确认方案后刷新打印表用）
    static PrintOutputData buildPrintOutput(const CalcInput &input, const CalcResult &result);

    // 电磁计算全链路：CalcInput（默认即 SB20-M-630-10）→ CalcResult
    bool calcElectromagnetic(const CalcInput &input, CalcResult &result) override;

    // 对拍自检：用 SB20-M-630-10 计算单缓存值验证全链路，返回逐项对照文本
    static QString selfTestReport();
};

#endif  // ELECTROMAGNETICENGINE_H
