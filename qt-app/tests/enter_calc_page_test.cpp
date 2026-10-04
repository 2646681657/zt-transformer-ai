#include "EnterCalcPage.h"
#include "EmResultPanel.h"
#include "GridOptimizer.h"
#include "PrintTableWidget.h"
#include "SchemeTableWidget.h"
#include <QApplication>
#include <QMetaObject>
#include <QTableWidget>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    EnterCalcPage page;
    auto *results = page.findChild<EmResultPanel *>();
    auto *schemes = page.findChild<SchemeTableWidget *>();
    auto *print = page.findChild<PrintTableWidget *>();
    if (!results || !schemes || !print) {
        qCritical() << "Calculation page widgets are missing";
        return 11;
    }

    CalcInput first;
    page.setCalcInput(first);
    if (!QMetaObject::invokeMethod(&page, "onRunEmCalc", Qt::DirectConnection)
            || schemes->rowCount() == 0
            || qobject_cast<QTableWidget *>(results->widget(0))->rowCount() == 0) {
        qCritical() << "Could not establish a completed first calculation";
        return 12;
    }

    CalcInput second = first;
    second.coreDiameter_mm += 5.0;
    page.setCalcInput(second);
    for (int i = 0; i < results->count(); ++i) {
        auto *table = qobject_cast<QTableWidget *>(results->widget(i));
        if (!table || table->rowCount() != 0) {
            qCritical() << "Old calculation remains visible in result tab" << i;
            return 13;
        }
    }
    if (schemes->rowCount() != 0) {
        qCritical() << "Old schemes remain available after input change";
        return 14;
    }
    if (print->rowCount() == 0) {
        qCritical() << "New input's initial print sheet was not prepared";
        return 15;
    }

    auto *optimizer = page.findChild<GridOptimizer *>();
    if (!optimizer) {
        return 16;
    }
    bool finished = false;
    QObject::connect(optimizer, &IOptimizer::finished, &page,
                     [&finished](bool, const OptimizeCandidate &, int, int) {
                         finished = true;
                     });
    if (!QMetaObject::invokeMethod(&page, "onOptimizeStart", Qt::DirectConnection)) {
        return 17;
    }
    CalcInput third = second;
    third.coreDiameter_mm += 5.0;
    page.setCalcInput(third);
    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 10000) {
        app.processEvents();
        QThread::msleep(10);
    }
    if (!finished || schemes->rowCount() != 0
            || qobject_cast<QTableWidget *>(results->widget(0))->rowCount() != 0) {
        qCritical() << "Old optimizer repopulated results after input change";
        return 18;
    }
    return 0;
}
