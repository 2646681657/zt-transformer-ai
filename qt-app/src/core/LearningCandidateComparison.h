#ifndef LEARNINGCANDIDATECOMPARISON_H
#define LEARNINGCANDIDATECOMPARISON_H

#include "IOptimizer.h"
#include <QJsonObject>

namespace LearningComparison {
// Independent, immutable display snapshot; never recalculates or trains.
QJsonObject makeInput(const QVector<OptimizeCandidate> &candidates, QString &error);
bool validateReport(const QJsonObject &input, const QJsonObject &report,
                    const QString &modelPath, const QString &inputPath, QString &error);
}
#endif
