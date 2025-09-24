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
#include "usefeaturewidget.h"
#include "resourcemanager.h"
#include "audioplayer.h"

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
    QWidget* m_centralWidgetContainer;  ///< Main container for the central widget
    QWidget* m_sidebar;                 ///< Sidebar widget for navigation buttons
    QVBoxLayout* m_sidebarLayout;       ///< Layout for sidebar elements
    QHBoxLayout* m_mainLayout;          ///< Main horizontal layout (sidebar + content)
    QStackedWidget* m_stackedContent;   ///< Stacked widget for different content pages
    QProgressBar* m_globalProgressBar;  ///< Global progress bar at the bottom
    AddSoundFeatureWidget* m_addSoundFeatureWidget;
    UseFeatureWidget* m_useFeatureWidget;
    AudioPlayer* m_audioPlayer;         ///< Audio player widget for playback control

    // Setup Methods
    void setupUI();       ///< Initializes the main UI components and layout
    void setupSidebar();  ///< Sets up the sidebar with navigation buttons
    void setupContent();  ///< Sets up the stacked content area with pages
    void setupPages();    ///< Creates and configures the content pages
    void setupConnections(); ///< Establishes signal-slot connections

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

    /**
     * @brief Slot to handle play request from file widgets.
     * @param filePath Path to the audio file to play.
     */
    void onPlayRequested(const QString& filePath);

    /**
     * @brief Slot to handle file addition for locking.
     * @param path The file path.
     * @param type The file type.
     */
    void onFileAdded(const QString& path, ResourceManager::FileType type);

    /**
     * @brief Slot to handle file removal for unlocking.
     * @param path The file path.
     * @param type The file type.
     */
    void onFileRemoved(const QString& path, ResourceManager::FileType type);

    /**
     * @brief Slot to handle folder removal for unlocking files.
     * @param folderPath The folder path.
     * @param type The file type.
     */
    void onFolderRemoved(const QString& folderPath, ResourceManager::FileType type);

    /**
     * @brief Slot to handle processing started.
     */
    void onProcessingStarted();

    /**
     * @brief Slot to handle processing finished.
     * @param results List of result file paths.
     */
    void onProcessingFinished(const QStringList& results = QStringList());

    /**
     * @brief Slot to handle processing error.
     * @param error Error message.
     */
    void onProcessingError(const QString& error);

};

#endif // MAINWINDOW_H
