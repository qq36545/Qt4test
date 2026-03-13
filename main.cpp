#include "mainwindow.h"
#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 固定中文（简体）
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));

    // 安装 Qt 基础翻译（覆盖 QMessageBox 标准按钮文本）
    QTranslator qtBaseTranslator;
    if (qtBaseTranslator.load("qtbase_zh_CN", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtBaseTranslator);
    }

    // 设置应用信息
    app.setApplicationName("Qt4test");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Qt4test");

    // 创建并显示主窗口
    MainWindow window;
    window.show();

    return app.exec();
}
