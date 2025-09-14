#include "mainwindow.h"
#include "fileutils.h"
#include <QMessageBox>

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
    globalProgressBar->setRange(0, 100);
    globalProgressBar->setValue(0);
    globalProgressBar->setTextVisible(false); // Hide percentage text
    globalProgressBar->setFixedHeight(18);    // Custom height
    mainVLayout->addWidget(globalProgressBar);

    centralWidgetContainer->setLayout(mainVLayout);
    setCentralWidget(centralWidgetContainer);

    setWindowTitle("Qt MainWindow with Left Sidebar");
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
    stackedContent = new QStackedWidget(this);

    QTextEdit* howToUsePage = new QTextEdit("This is the How to Use page.", this);
    addSoundFeatureWidget = new AddSoundFeatureWidget(this);
    UseFeatureWidget* useFeatureWidget = new UseFeatureWidget(this);

    howToUsePage->setReadOnly(true);

    stackedContent->addWidget(howToUsePage);                   // Index 0
    stackedContent->addWidget(addSoundFeatureWidget);          // Index 1
    stackedContent->addWidget(useFeatureWidget);                 // Index 2

    stackedContent->setCurrentIndex(0); // Default to "How to Use"

    // Connect to ResourceManager signals for locking
    ResourceManager* rm = ResourceManager::instance();
    connect(rm, &ResourceManager::fileAdded,
            this, [this, rm](const QString& path, ResourceManager::FileType type){
                if (type == ResourceManager::FileType::WavForFeature) {
                    if (!rm->lockFile(path)) {
                        QMessageBox::warning(this, "File Lock Error", "Failed to set file to read-only: " + path);
                    }
                }
            });
    connect(rm, &ResourceManager::fileRemoved,
            this, [this, rm](const QString& path, ResourceManager::FileType type){
                if (type == ResourceManager::FileType::WavForFeature) {
                    if (!rm->unlockFile(path)) {
                        QMessageBox::warning(this, "File Unlock Error", "Failed to remove read-only from file: " + path);
                    }
                }
            });

    connect(rm, &ResourceManager::folderRemoved,
            this, [this, rm](const QString& folderPath, ResourceManager::FileType type){
                if (type == ResourceManager::FileType::WavForFeature) {
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
                    if (!FileUtils::setFileReadOnly(folderPath, false)) {
                        // Ignore if fails, as folder might not be set
                    }
                }
            });

    // Connect progressUpdated signal to updateProgress slot
    connect(rm, &ResourceManager::progressUpdated, this, &MainWindow::updateProgress);

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
}

/**
 * @brief Slot to handle play request from file widgets.
 * @param filePath Path to the audio file to play.
 */
void MainWindow::onPlayRequested(const QString& filePath)
{
    audioPlayer->playAudio(filePath);
}

