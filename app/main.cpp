/**
 * @file main.cpp
 * @brief 程序入口：日志 → VTK 格式 → QApplication → ClientCore → 主窗口
 */
#include <cstdlib>
#include <unistd.h>

#include <QApplication>
#include <QCoreApplication>
#include <QSurfaceFormat>

#include <QVTKOpenGLNativeWidget.h>

#include "client_core.h"
#include "common/log_service.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);
    app.setApplicationName("CBCT Client");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("CBCT");
    app.setQuitOnLastWindowClosed(true);

    // 日志目录：<exe 目录>/log
    cbct::logging::LogService::instance().initialize(
        QCoreApplication::applicationDirPath());
    CBCT_LOG_INFO("app", "程序启动");

    cbct::ClientCore::instance().initialize();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        CBCT_LOG_INFO("app", "程序退出");
        cbct::logging::LogService::instance().flushAll();
        cbct::ClientCore::instance().shutdown();
    });

    MainWindow w;
    w.showMaximized();

    const int rc = app.exec();
    cbct::ClientCore::instance().shutdown();
    cbct::logging::LogService::instance().flushAll();

    // 跳过可能卡住的静态析构，保证 gdb/VSCode 调试能结束
    _exit(rc);
}
