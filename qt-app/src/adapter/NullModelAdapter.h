#ifndef NULLMODELADAPTER_H
#define NULLMODELADAPTER_H
// 空实现适配器（LLM未接入时的占位，所有方法返回空值）

#include "IModelAdapter.h"

class NullModelAdapter : public IModelAdapter {
public:
    QString suggestParameters(const QString &) override { return {}; }
    QString evaluateScheme(const QString &) override { return {}; }
};

#endif // NULLMODELADAPTER_H
