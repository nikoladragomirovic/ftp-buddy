#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    //Initiate list command upon opening for the first time
    w.on_button_list_clicked();
    return a.exec();
}
