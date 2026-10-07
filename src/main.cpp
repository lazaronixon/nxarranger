#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("NXArranger");
    QApplication::setOrganizationName("NXArranger");
    QApplication::setApplicationVersion(NXARRANGER_VERSION);

    MainWindow window;
    window.show();

    return app.exec();
}
