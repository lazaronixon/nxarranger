#include "MainWindow.h"

#include <QApplication>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("NXArranger");
    QApplication::setOrganizationName("NXArranger");

    MainWindow window;
    window.show();

    // Optional: open a file passed on the command line.
    const QStringList args = QApplication::arguments();
    if (args.size() > 1 && QFileInfo::exists(args.at(1)))
        window.openFile(args.at(1));

    return app.exec();
}
