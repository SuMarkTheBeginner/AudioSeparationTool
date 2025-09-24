/**
 * @file main.cpp
 * @brief Main entry point for the Audio Separation Tool application.
 *
 * This file initializes the Qt application, creates the main window,
 * displays it, and starts the event loop.
 */

#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QDebug>
#include <QResource>
#include "constants.h"

/**
 * @brief Main function.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return Exit code of the application.
 */
int main(int argc, char *argv[])
{
    qputenv("QML_DISABLE_OPTIMIZER", "1");
    QApplication a(argc, argv);

    // Register the models resource file
    QString rccPath = QDir::currentPath() + "/models.rcc";
    qDebug() << "Current working directory:" << QDir::currentPath();
    qDebug() << "Looking for models.rcc at:" << rccPath;
    qDebug() << "models.rcc exists:" << QFile::exists(rccPath);

    if (QFile::exists(rccPath)) {
        if (QResource::registerResource(rccPath)) {
            qDebug() << "SUCCESS: Successfully registered models.rcc resource file";
        } else {
            qDebug() << "ERROR: Failed to register models.rcc resource file";
        }
    } else {
        qDebug() << "ERROR: models.rcc file not found at:" << rccPath;
    }



    MainWindow w;
    w.show();
    return a.exec();
}
