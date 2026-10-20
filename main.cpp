#include <MainWindow.h>
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow mainWindow;

    mainWindow.setWindowTitle("CAEStudio");
    // mainWindow.resize(1500, 1000);
    mainWindow.show();

    return app.exec();
}