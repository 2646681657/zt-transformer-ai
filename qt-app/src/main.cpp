#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QProcessEnvironment>
#include "gui/MainWindow.h"
#include "engine/ElectromagneticEngine.h"

// 应用程序入口：初始化Qt、加载全局样式表、启动主窗口
// 环境变量 ZTF_EM_SELFTEST=1 时执行电磁计算对拍自检并退出（不启动界面）
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // 固定浅色基础调色板，避免未被样式表覆盖的控件继承系统深色主题。
    app.setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#F3F5F4"));
    palette.setColor(QPalette::WindowText, QColor("#24362B"));
    palette.setColor(QPalette::Base, QColor("#FFFFFF"));
    palette.setColor(QPalette::AlternateBase, QColor("#F7FAF8"));
    palette.setColor(QPalette::Text, QColor("#24362B"));
    palette.setColor(QPalette::Button, QColor("#EDF3EF"));
    palette.setColor(QPalette::ButtonText, QColor("#24362B"));
    palette.setColor(QPalette::Highlight, QColor("#E7F2EA"));
    palette.setColor(QPalette::HighlightedText, QColor("#185C37"));
    palette.setColor(QPalette::ToolTipBase, QColor("#FFFFFF"));
    palette.setColor(QPalette::ToolTipText, QColor("#24362B"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#607368"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#607368"));
    app.setPalette(palette);
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
