#include <QApplication>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CBCT Client");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("CBCT");

    MainWindow w;
    w.show();

    return app.exec();
}
