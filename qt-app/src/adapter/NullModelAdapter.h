#ifndef NULLMODELADAPTER_H
#define NULLMODELADAPTER_H

#include "IModelAdapter.h"

class NullModelAdapter : public IModelAdapter {
public:
    QString suggestParameters(const QString &) override { return {}; }
    QString evaluateScheme(const QString &) override { return {}; }
};

#endif // NULLMODELADAPTER_H
