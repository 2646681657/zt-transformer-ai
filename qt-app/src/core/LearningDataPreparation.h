#ifndef LEARNINGDATAPREPARATION_H
#define LEARNINGDATAPREPARATION_H
#include "SelfLearningDataset.h"
#include <QByteArray>
#include <QStringList>

namespace LearningPreparation {
QJsonObject inspect(const QVector<SelfLearning::Sample> &samples);
// UTF-8 BOM, RFC4180. Every data text cell has one reversible apostrophe
// prefix for Excel; a literal leading apostrophe therefore becomes two.
QByteArray csvTemplate(const QVector<SelfLearning::Sample> &samples);
struct CsvPreview {
    QVector<SelfLearning::Sample> originals, replacements;
    QStringList errors;
};
bool previewCsv(const QByteArray &bytes, const QVector<SelfLearning::Sample> &current,
                CsvPreview &preview, QString &error);
bool commitCsv(const QString &path, const CsvPreview &preview, QString &error);
}
#endif
