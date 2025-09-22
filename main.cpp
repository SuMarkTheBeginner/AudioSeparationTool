/**
 * @file main.cpp
 * @brief Main entry point for the Audio Separation Tool application.
 *
 * This file initializes the Qt application, creates the main window,
 * displays it, and starts the event loop.
 */

#include "mainwindow.h"
#include <QApplication>

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
    MainWindow w;
    w.show();
    return a.exec();
}
