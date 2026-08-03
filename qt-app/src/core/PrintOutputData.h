#ifndef PRINTOUTPUTDATA_H
#define PRINTOUTPUTDATA_H

#include <QString>
#include <QVector>

struct PrintOutputRow {
    QString leftName;
    QString leftValue;
    QString leftUnit;
    QString rightName;
    QString rightValue;
    QString rightUnit;
    bool isSectionHeader = false;
};

class PrintOutputData {
public:
    QVector<PrintOutputRow> rows;

    static PrintOutputData createDefault();
};

#endif // PRINTOUTPUTDATA_H
