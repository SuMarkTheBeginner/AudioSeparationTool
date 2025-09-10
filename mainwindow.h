#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QProgressBar>

#include "addsoundfeaturewidget.h"
#include "resourcemanager.h"

/**
 * @brief Main window class for the Audio Separation Tool application.
 *
 * This class represents the primary user interface window, featuring a sidebar
 * for navigation and a stacked content area for different pages. It manages
 * the overall layout and handles page switching based on user interactions.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the MainWindow.
     * @param parent The parent widget (default is nullptr).
     */
    explicit MainWindow(QWidget *parent = nullptr);

private:
    // UI Layout Components
    QWidget* centralWidgetContainer;  ///< Main container for the central widget
    QWidget* sidebar;                 ///< Sidebar widget for navigation buttons
    QVBoxLayout* sidebarLayout;       ///< Layout for sidebar elements
    QHBoxLayout* mainLayout;          ///< Main horizontal layout (sidebar + content)
    QStackedWidget* stackedContent;   ///< Stacked widget for different content pages
    QProgressBar* globalProgressBar;  ///< Global progress bar at the bottom
    AddSoundFeatureWidget* addSoundFeatureWidget;

    // Setup Methods
    void setupUI();       ///< Initializes the main UI components and layout
    void setupSidebar();  ///< Sets up the sidebar with navigation buttons
    void setupContent();  ///< Sets up the stacked content area with pages

private slots:
    /**
     * @brief Slot to show the "How to Use" page.
     */
    void showHowToUse();

    /**
     * @brief Slot to show the "Add Feature" page.
     */
    void showAddFeature();

    /**
     * @brief Slot to show the "Use Feature" page.
     */
    void showUseFeature();

    /**
     * @brief Slot to update the global progress bar.
     * @param value Progress value (0-100).
     */
    void updateProgress(int value);

};

#endif // MAINWINDOW_H
