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

    // Global progress bar fixed at the bottom
    globalProgressBar = new QProgressBar(this);
    globalProgressBar->setRange(0, 100);
    globalProgressBar->setValue(50);
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
    QTextEdit* useFeaturePage = new QTextEdit("This is the Use Existing Sound Feature page.", this);

    howToUsePage->setReadOnly(true);
    useFeaturePage->setReadOnly(true);

    stackedContent->addWidget(howToUsePage);                   // Index 0
    stackedContent->addWidget(addSoundFeatureWidget);          // Index 1
    stackedContent->addWidget(useFeaturePage);                 // Index 2

    stackedContent->setCurrentIndex(0); // Default to "How to Use"

    connect(addSoundFeatureWidget, &AddSoundFeatureWidget::fileAdded,
            this, [this](const QString& path){
                if (!FileUtils::setFileReadOnly(path, true)) {
                    QMessageBox::warning(this, "File Lock Error", "Failed to set file to read-only: " + path);
                } else {
                    lockedFiles.insert(path);
                }
            });
    connect(addSoundFeatureWidget, &AddSoundFeatureWidget::fileRemoved,
            this, [this](const QString& path){
                if (!FileUtils::setFileReadOnly(path, false)) {
                    QMessageBox::warning(this, "File Unlock Error", "Failed to remove read-only from file: " + path);
                } else {
                    lockedFiles.remove(path);
                }
            });

    connect(addSoundFeatureWidget, &AddSoundFeatureWidget::folderRemoved,
            this, [this](const QString& folderPath){
                // Unlock all files in the folder
                QSet<QString> toRemove;
                for (const QString& filePath : lockedFiles) {
                    if (filePath.startsWith(folderPath + "/")) {
                        if (!FileUtils::setFileReadOnly(filePath, false)) {
                            QMessageBox::warning(this, "File Unlock Error", "Failed to remove read-only from file: " + filePath);
                        }
                        toRemove.insert(filePath);
                    }
                }
                for (const QString& filePath : toRemove) {
                    lockedFiles.remove(filePath);
                }
                // Also unlock the folder itself if it was set
                if (!FileUtils::setFileReadOnly(folderPath, false)) {
                    // Ignore if fails, as folder might not be set
                }
            });
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

