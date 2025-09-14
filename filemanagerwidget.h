#ifndef FILEMANAGERWIDGET_H
#define FILEMANAGERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include "folderwidget.h"
#include "filewidget.h"
#include "resourcemanager.h"

/**
 * @brief Base class for widgets that manage files and folders.
 *
 * Provides common functionality for adding/removing folders and files,
 * with drag-and-drop support and status messages.
 */
class FileManagerWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the FileManagerWidget.
     * @param fileType The type of files managed by this widget.
     * @param parent The parent widget (default is nullptr).
     */
    explicit FileManagerWidget(ResourceManager::FileType fileType, QWidget* parent = nullptr);

    /**
     * @brief Gets the file type managed by this widget.
     * @return The file type.
     */
    ResourceManager::FileType fileType() const { return m_fileType; }

protected:
    /**
     * @brief Sets up the common UI components.
     * @param instructionText Text for the instruction label.
     * @param addFolderText Text for the add folder button.
     * @param addFileText Text for the add file button.
     */
    void setupCommonUI(const QString& instructionText, const QString& addFolderText, const QString& addFileText);

    /**
     * @brief Handles drag enter events for drag-and-drop functionality.
     * @param event The drag enter event.
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * @brief Handles drop events for drag-and-drop functionality.
     * @param event The drop event.
     */
    void dropEvent(QDropEvent *event) override;

    // UI Components
    QLabel* statusLabel;              ///< Label for status messages
    QScrollArea* scrollArea;          ///< Scrollable area for content
    QWidget* fileContainer;           ///< Container for file widgets
    QVBoxLayout* fileLayout;          ///< Layout for file widgets
    QPushButton* addFolderButton;     ///< Button to add folders
    QPushButton* addFileButton;       ///< Button to add files

private:
    ResourceManager::FileType m_fileType; ///< The type of files managed

    /**
     * @brief Adds a folder to the widget.
     * @param folderPath The path to the folder.
     */
    void addFolder(const QString& folderPath);

    /**
     * @brief Adds a single file to the widget.
     * @param filePath The path to the file.
     */
    void addSingleFile(const QString& filePath);

signals:
    void playRequested(const QString& filePath); ///< Signal emitted when play is requested
};

#endif // FILEMANAGERWIDGET_H
