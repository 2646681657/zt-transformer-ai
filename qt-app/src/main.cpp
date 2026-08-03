#include <QApplication>
#include <QFile>
#include "gui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ZTF-Designer");
    app.setApplicationVersion("2.0.0");

    QFile styleFile(":/styles/ztf_theme.qss");
    if (styleFile.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
