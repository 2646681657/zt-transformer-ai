#ifndef PRINTTABLEWIDGET_H
#define PRINTTABLEWIDGET_H

#include <QTableWidget>
#include "PrintOutputData.h"

class PrintTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit PrintTableWidget(QWidget *parent = nullptr);
    void loadData(const PrintOutputData &data);
};

#endif // PRINTTABLEWIDGET_H
