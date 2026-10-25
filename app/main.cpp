/**
 * @file main.cpp
 * @brief 程序入口：VTK OpenGL 格式 → QApplication → ClientCore → 主窗口
 */
#include <QApplication>
#include <QSurfaceFormat>

// VTK Qt 绑定：必须在 QApplication 之前设置默认 surface format，
// 否则 GLEW/OpenGL 上下文容易初始化失败（VMware 虚拟机上尤其常见）。
#include <QVTKOpenGLNativeWidget.h>

#include "client_core.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    // 与 QVTKOpenGLNativeWidget 要求一致的默认格式（Depth24 / DoubleBuffer 等）
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

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
