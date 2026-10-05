#ifndef COSTBASISNOTES_H
#define COSTBASISNOTES_H

#include <QString>

// 仅统一显示说明，不提供价格或计算逻辑。
namespace CostBasisNotes {
inline QString engine()
{
    return QStringLiteral("内置基价材料成本：含铁芯、高低压导线、绝缘油及油箱；不含报价费用、利润和税额。寻优按此材料合计选优，不受报价页调价影响。");
}
inline QString quote()
{
    return QStringLiteral("报价按可编辑单价重算材料成本，另计费用、利润及税额；修改报价参数不改变寻优选优结果或引擎内置基价成本。");
}
}

#endif // COSTBASISNOTES_H
