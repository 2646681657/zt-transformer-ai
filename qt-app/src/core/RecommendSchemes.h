#ifndef RECOMMENDSCHEMES_H
#define RECOMMENDSCHEMES_H
// 内置推荐方案表：按典型型号预置的设计变量组合。
// 当前为占位实现（SB20-M-630-10 默认方案单条），待完整推荐表数据后替换扩充

#include <QString>
#include <QVector>
#include "CalcInput.h"

namespace RecommendSchemes {

struct RecommendScheme {
    QString name;         // 方案名称（型号/特点）
    QString description;  // 简要说明
    CalcInput input;      // 完整设计变量
};

// 全部内置推荐方案（调用时即时构造，便于后续扩充为多型号表）
inline QVector<RecommendScheme> all()
{
    return {
        {
            QStringLiteral("SB20-M-630-10 标准方案"),
            QStringLiteral("默认设计变量（推荐表占位，待替换为完整推荐方案数据）"),
            CalcInput{}
        }
    };
}

} // namespace RecommendSchemes

#endif // RECOMMENDSCHEMES_H
