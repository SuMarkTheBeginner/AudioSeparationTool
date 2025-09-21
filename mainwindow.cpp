#include "mainwindow.h"
#include "fileutils.h"
#include "constants.h"
#include <QMessageBox>
#include <QTimer>

/**
 * @brief Constructs the MainWindow and initializes the UI.
 *
 * This constructor sets up the main window by calling setupUI() to create
 * the central widget, sidebar, content area, and progress bar.
 *
 * @param parent The parent widget (default is nullptr).
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
}

/**
 * @brief Sets up the main user interface components and layout.
 *
 * This method creates the central widget container with a vertical layout,
 * adds the main horizontal layout (sidebar + content), and places the
 * global progress bar at the bottom. It also sets the window title and size.
 */
void MainWindow::setupUI()
{
    centralWidgetContainer = new QWidget(this);
    QVBoxLayout* mainVLayout = new QVBoxLayout(centralWidgetContainer);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    // Main area: sidebar + content
    mainLayout = new QHBoxLayout();
    setupSidebar();
    setupContent();

    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(stackedContent, 1); // Stretch factor for content
    mainVLayout->addLayout(mainLayout, 1);    // Stretch to fill remaining space

    // Audio player control bar
    audioPlayer = new AudioPlayer(this);
    mainVLayout->addWidget(audioPlayer);

    // Global progress bar fixed at the bottom
    globalProgressBar = new QProgressBar(this);
    globalProgressBar->setRange(Constants::PROGRESS_RANGE_MIN, Constants::PROGRESS_RANGE_MAX);
    globalProgressBar->setValue(0);
    globalProgressBar->setTextVisible(false); // Hide percentage text initially
    globalProgressBar->setFixedHeight(Constants::PROGRESS_BAR_HEIGHT);
    globalProgressBar->setVisible(false);     // Hidden by default
    mainVLayout->addWidget(globalProgressBar);

    centralWidgetContainer->setLayout(mainVLayout);
    setCentralWidget(centralWidgetContainer);

    setWindowTitle("Audio Separation Tool");
    resize(800, 600);
}

/**
 * @brief Sets up the sidebar with navigation buttons.
 *
 * This method creates the sidebar widget, adds navigation buttons for
 * "How to Use", "Add Feature", and "Use Feature", and connects their
 * clicked signals to the corresponding slots for page switching.
 */
void MainWindow::setupSidebar()
{
    sidebar = new QWidget(this);
    sidebarLayout = new QVBoxLayout(sidebar);

    // Create navigation buttons
    QPushButton* howToUseBtn = new QPushButton("How to use this", sidebar);
    QPushButton* addFeatureBtn = new QPushButton("Add new sound feature", sidebar);
    QPushButton* useFeatureBtn = new QPushButton("Use existing sound feature", sidebar);

    sidebarLayout->addWidget(howToUseBtn);
    sidebarLayout->addWidget(addFeatureBtn);
    sidebarLayout->addWidget(useFeatureBtn);
    sidebarLayout->addStretch();
    sidebar->setLayout(sidebarLayout);
    sidebar->setFixedWidth(200); // Adjust sidebar width

    // Connect button signals to slots
    connect(howToUseBtn, &QPushButton::clicked, this, &MainWindow::showHowToUse);
    connect(addFeatureBtn, &QPushButton::clicked, this, &MainWindow::showAddFeature);
    connect(useFeatureBtn, &QPushButton::clicked, this, &MainWindow::showUseFeature);
}

/**
 * @brief Sets up the stacked content area with different pages.
 *
 * This method creates a QStackedWidget and adds three pages:
 * - Index 0: "How to Use" page (QTextEdit)
 * - Index 1: "Add Sound Feature" page (AddSoundFeatureWidget)
 * - Index 2: "Use Existing Sound Feature" page (QTextEdit)
 * The default page is set to index 0.
 */
void MainWindow::setupContent()
{
    setupPages();
    setupConnections();
}

/**
 * @brief Creates and configures the content pages.
 */
void MainWindow::setupPages()
{
    stackedContent = new QStackedWidget(this);

    QTextEdit* howToUsePage = new QTextEdit("This is the How to Use page.", this);
    addSoundFeatureWidget = new AddSoundFeatureWidget(this);
    useFeatureWidget = new UseFeatureWidget(this);

    howToUsePage->setReadOnly(true);

    stackedContent->addWidget(howToUsePage);                   // Index 0
    stackedContent->addWidget(addSoundFeatureWidget);          // Index 1
    stackedContent->addWidget(useFeatureWidget);                 // Index 2

    stackedContent->setCurrentIndex(0); // Default to "How to Use"
}

/**
 * @brief Establishes signal-slot connections.
 */
void MainWindow::setupConnections()
{
    ResourceManager* rm = ResourceManager::instance();
    connect(rm, &ResourceManager::fileAdded, this, &MainWindow::onFileAdded);
    connect(rm, &ResourceManager::fileRemoved, this, &MainWindow::onFileRemoved);
    connect(rm, &ResourceManager::folderRemoved, this, &MainWindow::onFolderRemoved);

    // Connect progress signals to handle progress bar visibility and updates
    connect(rm, &ResourceManager::processingStarted, this, &MainWindow::onProcessingStarted);
    connect(rm, &ResourceManager::processingProgress, this, &MainWindow::updateProgress);
    connect(rm, &ResourceManager::processingFinished, this, &MainWindow::onProcessingFinished);
    connect(rm, &ResourceManager::processingError, this, &MainWindow::onProcessingError);

    // Connect playRequested from AddSoundFeatureWidget
    connect(addSoundFeatureWidget, &AddSoundFeatureWidget::playRequested, this, &MainWindow::onPlayRequested);
    // Connect playRequested from UseFeatureWidget
    connect(useFeatureWidget, &UseFeatureWidget::playRequested, this, &MainWindow::onPlayRequested);

    // Connect featuresUpdated signal to refresh UseFeatureWidget features
    connect(rm, &ResourceManager::featuresUpdated, useFeatureWidget, &UseFeatureWidget::refreshFeatures);
}

// Slot implementations

/**
 * @brief Slot to switch to the "How to Use" page.
 */
void MainWindow::showHowToUse()
{
    stackedContent->setCurrentIndex(0);
}

/**
 * @brief Slot to switch to the "Add Feature" page.
 */
void MainWindow::showAddFeature()
{
    stackedContent->setCurrentIndex(1);
}

/**
 * @brief Slot to switch to the "Use Feature" page.
 */
void MainWindow::showUseFeature()
{
    stackedContent->setCurrentIndex(2);
}

/**
 * @brief Slot to update the global progress bar.
 * @param value Progress value (0-100).
 */
void MainWindow::updateProgress(int value)
{
    globalProgressBar->setValue(value);
    // Ensure progress bar remains visible and text is shown during updates
    if (!globalProgressBar->isVisible()) {
        globalProgressBar->setVisible(true);
        globalProgressBar->setTextVisible(true);
    }
}

/**
 * @brief Slot to handle play request from file widgets.
 * @param filePath Path to the audio file to play.
 */
void MainWindow::onPlayRequested(const QString& filePath)
{
    audioPlayer->playAudio(filePath);
}

/**
 * @brief Slot to handle file addition for locking.
 * @param path The file path.
 * @param type The file type.
 */
void MainWindow::onFileAdded(const QString& path, ResourceManager::FileType type)
{
    if (type == ResourceManager::FileType::WavForFeature) {
        ResourceManager* rm = ResourceManager::instance();
        if (!rm->lockFile(path)) {
            QMessageBox::warning(this, "File Lock Error", "Failed to set file to read-only: " + path);
        }
    }
}

/**
 * @brief Slot to handle file removal for unlocking.
 * @param path The file path.
 * @param type The file type.
 */
void MainWindow::onFileRemoved(const QString& path, ResourceManager::FileType type)
{
    if (type == ResourceManager::FileType::WavForFeature) {
        ResourceManager* rm = ResourceManager::instance();
        if (!rm->unlockFile(path)) {
            QMessageBox::warning(this, "File Unlock Error", "Failed to remove read-only from file: " + path);
        }
    }
}

/**
 * @brief Slot to handle folder removal for unlocking files.
 * @param folderPath The folder path.
 * @param type The file type.
 */
void MainWindow::onFolderRemoved(const QString& folderPath, ResourceManager::FileType type)
{
    if (type == ResourceManager::FileType::WavForFeature) {
        ResourceManager* rm = ResourceManager::instance();
        // Unlock all files in the folder
        QSet<QString> addedFiles = rm->getAddedFiles(type);
        for (const QString& filePath : addedFiles) {
            if (filePath.startsWith(folderPath + "/")) {
                if (!rm->unlockFile(filePath)) {
                    QMessageBox::warning(this, "File Unlock Error", "Failed to remove read-only from file: " + filePath);
                }
            }
        }
        // Also unlock the folder itself if it was set
        if (FileUtils::setFileReadOnly(folderPath, false) != FileUtils::FileOperationResult::Success) {
            // Ignore if fails, as folder might not be set
        }
    }
}

/**
 * @brief Slot to handle processing started.
 */
void MainWindow::onProcessingStarted()
{
    globalProgressBar->setValue(0);
    globalProgressBar->setVisible(true);
    globalProgressBar->setTextVisible(true);
    globalProgressBar->setFormat("Processing... %p%");
}


/**
 * @brief Slot to handle processing finished.
 * @param results List of result file paths.
 */
void MainWindow::onProcessingFinished(const QStringList& results)
{
    globalProgressBar->setValue(100);
    globalProgressBar->setFormat("Completed! %p%");
    // Hide progress bar after a short delay to show completion
    QTimer::singleShot(2000, this, [this]() {
        globalProgressBar->setVisible(false);
        globalProgressBar->setTextVisible(false);
    });
}

/**
 * @brief Slot to handle processing error.
 * @param error Error message.
 */
void MainWindow::onProcessingError(const QString& error)
{
    globalProgressBar->setValue(100);
    globalProgressBar->setFormat("Error! %p%");
    // Hide progress bar after showing error briefly
    QTimer::singleShot(3000, this, [this]() {
        globalProgressBar->setVisible(false);
        globalProgressBar->setTextVisible(false);
    });
    QMessageBox::critical(this, "Processing Error", error);
}
