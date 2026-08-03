#ifndef IMODELADAPTER_H
#define IMODELADAPTER_H

#include <QString>
#include "TransformerParams.h"

class IModelAdapter {
public:
    virtual ~IModelAdapter() = default;
    virtual QString suggestParameters(const QString &context) = 0;
    virtual QString evaluateScheme(const QString &schemeDescription) = 0;
};

#endif // IMODELADAPTER_H
