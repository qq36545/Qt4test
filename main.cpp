#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用信息
    app.setApplicationName("Qt4test");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Qt4test");

    // 创建并显示主窗口
    MainWindow window;
    window.show();

    return app.exec();
}
