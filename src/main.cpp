#include "MainWindow.hpp"
#include <QApplication>
#include <QFile> // For loading stylesheet
#include <QDebug> // For error message
#include "MainWindow.hpp"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Load and apply stylesheet
    QFile styleFile(":/icon/stylesheet.qss"); // Correct resource path with prefix
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        qInfo("Loading stylesheet from resources...");
        QString styleSheet = QLatin1String(styleFile.readAll());
        a.setStyleSheet(styleSheet);
        styleFile.close();
    } else {
        qWarning("Could not load stylesheet.qss");
        // Try loading directly from src/ for development builds if not in resources
        QFile devStyleFile("src/stylesheet.qss");
        if (devStyleFile.open(QFile::ReadOnly | QFile::Text)) {
             QString styleSheet = QLatin1String(devStyleFile.readAll());
             a.setStyleSheet(styleSheet);
             devStyleFile.close();
             qInfo("Loaded stylesheet directly from src/");
        } else {
             qWarning("Could not load stylesheet from src/ either.");
        }
    }


    MainWindow w;
    w.show();

    return a.exec();
}
