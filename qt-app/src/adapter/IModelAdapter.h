#ifndef IMODELADAPTER_H
#define IMODELADAPTER_H
// LLM适配器接口（对接大语言模型实现参数推荐和方案评估）

#include <QString>
#include "TransformerParams.h"

class IModelAdapter {
public:
    virtual ~IModelAdapter() = default;
    // 根据上下文调用LLM推荐参数
    virtual QString suggestParameters(const QString &context) = 0;
    // 调用LLM评估优化方案的合理性
    virtual QString evaluateScheme(const QString &schemeDescription) = 0;
};

#endif // IMODELADAPTER_H
