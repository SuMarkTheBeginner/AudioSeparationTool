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
    m_centralWidgetContainer = new QWidget(this);
    QVBoxLayout* mainVLayout = new QVBoxLayout(m_centralWidgetContainer);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    // Main area: sidebar + content
    m_mainLayout = new QHBoxLayout();
    setupSidebar();
    setupContent();

    m_mainLayout->addWidget(m_sidebar);
    m_mainLayout->addWidget(m_stackedContent, 1); // Stretch factor for content
    mainVLayout->addLayout(m_mainLayout, 1);    // Stretch to fill remaining space

    // Audio player control bar
    m_audioPlayer = new AudioPlayer(this);
    mainVLayout->addWidget(m_audioPlayer);

    // Global progress bar fixed at the bottom
    m_globalProgressBar = new QProgressBar(this);
    m_globalProgressBar->setRange(Constants::PROGRESS_RANGE_MIN, Constants::PROGRESS_RANGE_MAX);
    m_globalProgressBar->setValue(0);
    m_globalProgressBar->setTextVisible(false); // Hide percentage text initially
    m_globalProgressBar->setFixedHeight(Constants::PROGRESS_BAR_HEIGHT);
    m_globalProgressBar->setVisible(false);     // Hidden by default
    mainVLayout->addWidget(m_globalProgressBar);

    m_centralWidgetContainer->setLayout(mainVLayout);
    setCentralWidget(m_centralWidgetContainer);

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
    m_sidebar = new QWidget(this);
    m_sidebarLayout = new QVBoxLayout(m_sidebar);

    // Create navigation buttons
    QPushButton* howToUseBtn = new QPushButton("How to use this", m_sidebar);
    QPushButton* addFeatureBtn = new QPushButton("Add new sound feature", m_sidebar);
    QPushButton* useFeatureBtn = new QPushButton("Use existing sound feature", m_sidebar);

    m_sidebarLayout->addWidget(howToUseBtn);
    m_sidebarLayout->addWidget(addFeatureBtn);
    m_sidebarLayout->addWidget(useFeatureBtn);
    m_sidebarLayout->addStretch();
    m_sidebar->setLayout(m_sidebarLayout);
    m_sidebar->setFixedWidth(200); // Adjust sidebar width

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
    m_stackedContent = new QStackedWidget(this);

    QString howToUseContent = R"(
<html>
<head>
    <style>
        :root {
            --text-color: #333;
            --bg-color: transparent;
            --h1-color: #2c3e50;
            --h2-color: #2980b9;
            --note-bg: #ecf0f1;
            --note-border: #3498db;
            --warning-bg: #ffeaa7;
            --warning-border: #d63031;
        }

        @media (prefers-color-scheme: dark) {
            :root {
                --text-color: #e1e1e1;
                --bg-color: transparent;
                --h1-color: #87ceeb;
                --h2-color: #63b8ff;
                --note-bg: #37474f;
                --note-border: #42a5f5;
                --warning-bg: #4e342e;
                --warning-border: #ff5722;
            }
        }

        body {
            font-family: 'Microsoft YaHei UI', 'PingFang SC', 'Hiragino Sans GB', 'SimHei', Arial, sans-serif;
            margin: 10px;
            color: var(--text-color);
            background-color: var(--bg-color);
        }

        h1 {
            color: var(--h1-color);
            border-bottom: 2px solid var(--h2-color);
            padding-bottom: 5px;
            font-weight: 600;
        }

        h2 {
            color: var(--h2-color);
            margin-top: 20px;
            font-weight: 500;
        }

        h3 {
            color: var(--h2-color);
            margin-top: 15px;
            font-size: 1.1em;
        }

        p {
            line-height: 1.6;
            margin-bottom: 10px;
        }

        ul {
            margin: 10px 0;
            padding-left: 20px;
        }

        li {
            margin-bottom: 5px;
        }

        .note {
            background-color: var(--note-bg);
            padding: 12px;
            border-left: 4px solid var(--note-border);
            margin: 10px 0;
            border-radius: 4px;
            font-weight: 500;
        }

        .warning {
            background-color: var(--warning-bg);
            padding: 12px;
            border-left: 4px solid var(--warning-border);
            margin: 10px 0;
            border-radius: 4px;
            font-weight: 500;
        }

        strong {
            color: var(--h1-color);
        }
    </style>
</head>
<body>
    <h1>Audio Separation Tool 使用指南</h1>

    <p>歡迎使用 Audio Separation Tool！這是一個用於音頻分離的強大工具，可以幫助您從複合音頻中提取特定的聲音元素。</p>

    <h2>主要功能</h2>
    <ul>
        <li><strong>新增聲音特徵 (Add Sound Feature)</strong>：訓練AI模型學習特定聲音類別</li>
        <li><strong>使用現有聲音特徵 (Use Existing Feature)</strong>：應用已訓練的模型進行音頻分離</li>
    </ul>

    <h2>使用步驟</h2>

    <h3>1. 新增聲音特徵</h3>
    <p>如果您要訓練AI學習新的聲音類別，請使用此功能：</p>
    <ol>
        <li>點擊側邊欄的「Add new sound feature」按鈕</li>
        <li>選擇或創建一個新的特徵目錄</li>
        <li>上傳包含目標聲音的WAV音頻文件</li>
        <li>選擇輸出目錄</li>
        <li>點擊「Start」按鈕開始處理</li>
    </ol>
    <div class="note">
        <strong>提示：</strong>為了獲得最佳效果，請上傳至少5-10個包含目標聲音的音頻文件，並確保音頻質量良好。
    </div>

    <h3>2. 使用現有聲音特徵</h3>
    <p>當您已經有訓練好的聲音特徵時，使用此功能進行音頻分離：</p>
    <ol>
        <li>點擊側邊欄的「Use existing sound feature」按鈕</li>
        <li>選擇包含音頻特徵的目錄</li>
        <li>上傳待分離的音頻文件（可以是音頻文件或包含音頻的目錄）</li>
        <li>選擇輸出目錄</li>
        <li>點擊「Start」按鈕開始分離過程</li>
    </ol>

    <h2>重要注意事項</h2>
    <div class="warning">
        <strong>警告：</strong>某些上傳的音頻文件會在處理過程中被鎖定為只讀狀態，以防止意外修改。如需刪除這些文件，請確保先完成相關處理。
    </div>

    <h2>音頻格式支援</h2>
    <ul>
        <li><strong>輸入格式：</strong>WAV, MP3, FLAC, OGG 等常見音頻格式</li>
        <li><strong>輸出格式：</strong>處理後的音頻會以WAV格式保存</li>
        <li><strong>推薦設置：</strong>44.1kHz採樣率，16位深度，單聲道或立體聲</li>
    </ul>

    <h2>系統需求</h2>
    <ul>
        <li>Windows 10 或更高版本</li>
        <li>至少 4GB RAM</li>
        <li>支援 CUDA 的 GPU（推薦，用於加速處理）</li>
        <li>足夠的磁碟空間用於音頻處理和模型存儲</li>
    </ul>

    <h2>故障排除</h2>
    <ul>
        <li><strong>處理失敗：</strong>檢查音頻文件是否損壞，或確保有足夠的磁碟空間</li>
        <li><strong>記憶體錯誤：</strong>嘗試減少同時處理的音頻文件數量</li>
        <li><strong>GPU 錯誤：</strong>確保安裝了正確的 CUDA 驅動程序</li>
    </ul>

    <p>如果您遇到任何問題，請檢查應用程式的狀態欄獲取詳細錯誤信息。</p>
</body>
</html>
    )";

    QTextEdit* howToUsePage = new QTextEdit(howToUseContent, this);
    m_addSoundFeatureWidget = new AddSoundFeatureWidget(this);
    m_useFeatureWidget = new UseFeatureWidget(this);

    howToUsePage->setReadOnly(true);

    m_stackedContent->addWidget(howToUsePage);                   // Index 0
    m_stackedContent->addWidget(m_addSoundFeatureWidget);          // Index 1
    m_stackedContent->addWidget(m_useFeatureWidget);                 // Index 2

    m_stackedContent->setCurrentIndex(0); // Default to "How to Use"
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
    connect(rm, &ResourceManager::separationProcessingFinished, this, &MainWindow::onProcessingFinished);
    connect(rm, &ResourceManager::processingError, this, &MainWindow::onProcessingError);

    // Connect playRequested from AddSoundFeatureWidget
    connect(m_addSoundFeatureWidget, &AddSoundFeatureWidget::playRequested, this, &MainWindow::onPlayRequested);
    // Connect playRequested from UseFeatureWidget
    connect(m_useFeatureWidget, &UseFeatureWidget::playRequested, this, &MainWindow::onPlayRequested);

    // Connect featuresUpdated signal to refresh UseFeatureWidget features
    connect(rm, &ResourceManager::featuresUpdated, m_useFeatureWidget, &UseFeatureWidget::refreshFeatures);
}

// Slot implementations

/**
 * @brief Slot to switch to the "How to Use" page.
 */
void MainWindow::showHowToUse()
{
    m_stackedContent->setCurrentIndex(0);
}

/**
 * @brief Slot to switch to the "Add Feature" page.
 */
void MainWindow::showAddFeature()
{
    m_stackedContent->setCurrentIndex(1);
}

/**
 * @brief Slot to switch to the "Use Feature" page.
 */
void MainWindow::showUseFeature()
{
    m_stackedContent->setCurrentIndex(2);
}

/**
 * @brief Slot to update the global progress bar.
 * @param value Progress value (0-100).
 */
void MainWindow::updateProgress(int value)
{
    m_globalProgressBar->setValue(value);
    // Ensure progress bar remains visible and text is shown during updates
    if (!m_globalProgressBar->isVisible()) {
        m_globalProgressBar->setVisible(true);
        m_globalProgressBar->setTextVisible(true);
    }
}

/**
 * @brief Slot to handle play request from file widgets.
 * @param filePath Path to the audio file to play.
 */
void MainWindow::onPlayRequested(const QString& filePath)
{
    m_audioPlayer->playAudio(filePath);
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
    m_globalProgressBar->setValue(0);
    m_globalProgressBar->setVisible(true);
    m_globalProgressBar->setTextVisible(true);
    m_globalProgressBar->setFormat("Processing... %p%");
}


/**
 * @brief Slot to handle processing finished.
 * @param results List of result file paths.
 */
void MainWindow::onProcessingFinished(const QStringList& results)
{
    m_globalProgressBar->setValue(100);
    // Hide progress bar and text immediately when processing finishes
    m_globalProgressBar->setVisible(false);
    m_globalProgressBar->setTextVisible(false);
    m_globalProgressBar->setFormat("Processing... %p%"); // Reset format for next use
}

/**
 * @brief Slot to handle processing error.
 * @param error Error message.
 */
void MainWindow::onProcessingError(const QString& error)
{
    m_globalProgressBar->setValue(100);
    m_globalProgressBar->setFormat("Error! %p%");
    // Hide progress bar after showing error briefly
    QTimer::singleShot(3000, this, [this]() {
        m_globalProgressBar->setVisible(false);
        m_globalProgressBar->setTextVisible(false);
    });
    QMessageBox::critical(this, "Processing Error", error);
}
