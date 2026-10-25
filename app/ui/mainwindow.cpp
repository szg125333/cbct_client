#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "main_toolbar.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->mainToolbar, &MainToolbar::actionTriggered,
            this, [this](const QString &actionName) {
                statusBar()->showMessage(actionName, 3000);
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}
