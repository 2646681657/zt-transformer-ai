#ifndef PARAMTABLEWIDGET_H
#define PARAMTABLEWIDGET_H

#include <QTableWidget>
#include "TransformerParams.h"

class ParamTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit ParamTableWidget(QWidget *parent = nullptr);
    void loadParams(const TransformerParams &params);
    TransformerParams getParams() const;

private:
    void setupTable();
    void addSectionRow(int row, const QString &title);
    void addParamRow(int row, const QString &name, const QString &value,
                     const QString &optName = {}, const QString &optValue = {});
};

#endif // PARAMTABLEWIDGET_H
