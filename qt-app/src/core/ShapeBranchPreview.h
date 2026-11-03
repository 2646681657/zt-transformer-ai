#ifndef SHAPEBRANCHPREVIEW_H
#define SHAPEBRANCHPREVIEW_H

#include <QMap>
#include <QPair>
#include <QString>

// Deliberately not a CalcResult: partial worksheet evidence must never become
// an optimizer candidate, a confirmed scheme or a passing thermal assessment.
struct ShapeBranchPreview {
    bool geometryAvailable = false;
    bool windingAvailable = false;
    QMap<QString, QPair<double, int>> values;
    QMap<QString, QString> texts;
    QString note;
    QString error;
};

#endif
