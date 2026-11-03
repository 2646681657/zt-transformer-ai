#ifndef SELFLEARNINGDATASET_H
#define SELFLEARNINGDATASET_H

#include "CalcInput.h"
#include <QDate>
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <array>
#include <optional>

// Separate from CalcResult: no learned value is accepted by the production engine.
namespace SelfLearning {
constexpr int MetricCount = 6;
struct Metric { const char *key; const char *name; const char *unit; };
const std::array<Metric, MetricCount> &metrics();
QString engineVersion();
QString datasetPath();

struct Sample {
    QString id, productId, schemeName, source;
    QString purpose = QStringLiteral("unassigned");
    QDate testedOn;
    CalcInput input;
    std::array<double, MetricCount> baseline{};
    std::array<std::optional<double>, MetricCount> measured{};
};

bool makeSample(const CalcInput &input, Sample &out, QString &error);
QJsonObject encode(const Sample &sample);
bool decode(const QJsonObject &json, Sample &out, QString &error);
QJsonObject document(const QVector<Sample> &samples);
bool load(const QString &path, QVector<Sample> &out, QString &error);
bool save(const QString &path, const QVector<Sample> &samples, QString &error);
bool importDataset(const QString &path, const QByteArray &bytes, QString &error);
// original is empty for a new sample, otherwise the last loaded JSON snapshot.
// Conflict checking and persistence happen under the same inter-process lock.
bool updateSample(const QString &path, const Sample &sample, const QJsonObject &original, QString &error);
QString purposeName(QString purpose);
bool validateDataset(const QVector<Sample> &samples, QString &error);
// expectedGroup must contain every current record of the canonical product.
bool assignProductPurpose(const QString &path, const QString &productId, const QString &purpose,
                          const QVector<Sample> &expectedGroup, QString &error);
// Update only: matching IDs and original snapshots are checked under the writer lock.
bool updateBatch(const QString &path, const QVector<Sample> &replacements,
                 const QVector<Sample> &originals, QString &error);
bool writeJson(const QString &path, const QJsonObject &json, QString &error);
bool trainingBundle(const QVector<Sample> &samples, QJsonObject &out, QString &error);
bool evaluationBundle(const QVector<Sample> &samples, QJsonObject &out, QString &error);
// Freeze from disk under the dataset writer lock; reject any stale dataset snapshot.
bool freezeDatasetBundle(const QString &path, const QVector<Sample> &expectedDataset,
                         const QStringList &selectedIds, bool evaluation, QJsonObject &out, QString &error);
std::array<int, MetricCount> labelledCounts(const QVector<Sample> &samples);
QString trainingBlockReason(const QVector<Sample> &samples);
QString correctionUnavailableReason();
}
#endif
