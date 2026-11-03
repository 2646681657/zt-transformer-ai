#pragma once
#include <QJsonObject>
#include <QStringList>

// Local audit for independent display only; never authorizes production
// calculation.
namespace LearningReview {
bool savePolicy(const QString &directory, QJsonObject policy, QString &error,
                QString expectedPolicySha256 = {});
QJsonObject policy(const QString &directory, QString &error);
bool approve(const QString &directory, QString modelPath,
             QString evaluationPath, QString reviewer, QString reason,
             QString &error, QString expectedPolicySha256 = {});
bool activate(const QString &directory, QString modelPath, QString &error);
bool deactivate(const QString &directory, QString reason, QString &error);
bool rollback(const QString &directory, QString &error);
QString activeModel(const QString &directory, QString &error);
QStringList blockers(const QString &directory, QString modelPath,
                     QString evaluationPath);
QJsonObject state(const QString &directory, QString &error);
bool canActivate(const QString &directory, QString modelPath, QString &error);
bool canRollback(const QString &directory, QString &error);
} // namespace LearningReview
