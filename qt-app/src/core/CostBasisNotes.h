#ifndef COSTBASISNOTES_H
#define COSTBASISNOTES_H

#include <QString>

// 仅统一显示说明，不提供价格或计算逻辑。
namespace CostBasisNotes {
inline QString engine()
{
    return QStringLiteral("材料成本含铁芯、导线、油和油箱，不含费用、利润及税额。默认计算单内置基价；可按牌号自定义硅钢价，其余材料仍用内置基价。采用口径随候选保存，不受报价页调价影响。");
}
inline QString engine(bool custom, double price, const QString &grade)
{
    return (custom ? QStringLiteral("自定义硅钢价：%1，%2元/kg；其余材料沿用内置基价。")
                   : QStringLiteral("计算单内置基价：硅钢%1，%2元/kg。"))
        .arg(grade).arg(price, 0, 'g', 15)
        + QStringLiteral("材料合计含铁芯、导线、油和油箱，不含费用、利润及税额；报价页独立，不自动同步。仅该价格口径下比较，不代表实际采购全成本或全局最优。");
}
inline QString quote()
{
    return QStringLiteral("报价按报价页单价独立重算，另计费用、利润及税额；不自动采用候选的自定义硅钢价，修改报价参数也不改变候选的冻结成本或寻优选优结果。");
}
}

#endif // COSTBASISNOTES_H
