#ifndef PRINTOUTPUTDATA_H
#define PRINTOUTPUTDATA_H
// 输出打印数据（左右双栏格式的计算结果展示行）

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

    // 创建包含默认演示数据的输出对象
    static PrintOutputData createDefault();
};

#endif // PRINTOUTPUTDATA_H
