#ifndef SCHEMETABLEWIDGET_H
#define SCHEMETABLEWIDGET_H

#include <QTableWidget>
#include <QVector>
#include "OptimizationResult.h"

class SchemeTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit SchemeTableWidget(QWidget *parent = nullptr);
    void addResult(const OptimizationResult &result);
    void clearResults();

private:
    void setupColumns();
};

#endif // SCHEMETABLEWIDGET_H
