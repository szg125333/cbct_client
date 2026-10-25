/**
 * @file main.cpp
 * @brief 程序入口：创建 QApplication → 初始化 ClientCore → 显示主窗口
 */
#include <QApplication>
#include "client_core.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 组织名/应用名决定 QStandardPaths 下数据目录：
    //   ~/.local/share/CBCT/CBCT Client/
    app.setApplicationName("CBCT Client");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("CBCT");

    // 必须在 setApplicationName 之后：RecipeManager 会用 AppLocalDataLocation
    cbct::ClientCore::instance().initialize();

    MainWindow w;
    w.show();

    return app.exec(); // 进入 Qt 事件循环
}
