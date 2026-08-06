#include <QApplication>
#include <QFile>
#include <QIcon>
#include "gui/MainWindow.h"

// 应用程序入口：初始化Qt、加载全局样式表、启动主窗口
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ZTBLD-Designer");
    app.setApplicationVersion("2.0.0");
    app.setWindowIcon(QIcon(":/icons/app_icon.svg"));

    QFile styleFile(":/styles/ztf_theme.qss");
    if (styleFile.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
