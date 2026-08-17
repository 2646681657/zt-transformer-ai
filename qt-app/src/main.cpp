#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QProcessEnvironment>
#include "gui/MainWindow.h"
#include "engine/ElectromagneticEngine.h"

// 应用程序入口：初始化Qt、加载全局样式表、启动主窗口
// 环境变量 ZTF_EM_SELFTEST=1 时执行电磁计算对拍自检并退出（不启动界面）
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ZTBLD-Designer");
    app.setApplicationVersion("2.0.0");
    app.setWindowIcon(QIcon(":/icons/app_icon.svg"));

    if (QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("ZTF_EM_SELFTEST")) == QLatin1String("1")) {
        // WIN32 子系统无控制台，对拍报告写入当前目录文件
        const QString report = ElectromagneticEngine::selfTestReport();
        QFile outFile(QStringLiteral("em_selftest_report.txt"));
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            outFile.write(report.toUtf8());
            outFile.close();
        }
        return 0;
    }

    QFile styleFile(":/styles/ztf_theme.qss");
    if (styleFile.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
