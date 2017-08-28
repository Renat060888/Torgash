
// qt
#include <QApplication>
// project
#include "mainwindow.h"

using namespace std;

int main( int argc, char *argv[] ){

    QApplication a( argc, argv );    

    MainWindow win;
    win.init();
    win.show();

    return a.exec();
}
