#ifndef HVCOILFORMNOTES_H
#define HVCOILFORMNOTES_H

#include <QString>

// 仅统一显示名称及工艺说明，不决定计算支持范围，不换算层数或匝数。
namespace HvCoilFormNotes {

inline QString name(int index)
{
    if (index == 1) return QStringLiteral("多层圆筒式");
    if (index == 2) return QStringLiteral("分段圆筒式（两段串联）");
    return QStringLiteral("未知高压线圈型式（%1）").arg(index);
}

inline QString processNote()
{
    return QStringLiteral("原计算单AC10批注：分段圆筒式按串联计算；Y9批注：分段时两段算一层。"
                          "这是工艺口径说明，不代表分段型式已验证支持；不要额外将层数或每层匝数除以2。");
}

inline QString layerNote()
{
    return QStringLiteral("W12为高压总层数；每层匝数Y9=INT(最高分接匝数/W12)+1，由计算得到，不在此手填。")
        + processNote();
}

inline QString unavailableReason()
{
    return QStringLiteral("分段圆筒式（两段串联）及其他高压线圈暂不开放计算："
                          "尚缺分段参考算例，工艺说明和公式准备不代表已验证支持。");
}

} // namespace HvCoilFormNotes

#endif // HVCOILFORMNOTES_H
