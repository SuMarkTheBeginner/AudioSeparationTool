#ifndef ADDSOUNDFEATUREWIDGET_H
#define ADDSOUNDFEATUREWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSet>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "folderwidget.h"
#include "filewidget.h"

/**
 * @brief Widget for adding sound features by selecting folders or dragging WAV files.
 *
 * This widget allows users to select folders containing WAV files or drag individual
 * WAV files into the interface. It organizes them into FolderWidget and FileWidget
 * components, provides sorting options, and supports drag-and-drop functionality.
 */
class AddSoundFeatureWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the AddSoundFeatureWidget.
     * @param parent The parent widget (default is nullptr).
     */
    explicit AddSoundFeatureWidget(QWidget *parent = nullptr);

    /**
     * @brief Enumeration for sorting types.
     */
    enum class SortType {
        NameAsc,    ///< Sort by name ascending
        NameDesc,   ///< Sort by name descending
        CreatedAsc, ///< Sort by creation time ascending
        CreatedDesc ///< Sort by creation time descending
    };

    /**
     * @brief Sorts all folders and single files based on the specified type.
     * @param type The sorting criteria.
     */
    void sortAll(SortType type);

    /**
     * @brief Gets the output file name from the input field.
     * @return The file name entered by the user.
     */
    QString getOutputFileName() const;

protected:
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

private:
    // UI Layout Components
    QVBoxLayout* layout;         ///< Main vertical layout
    QLabel* infoLabel;           ///< Information label for user instructions
    QLabel* statusLabel;         ///< Status label for error messages and feedback
    QScrollArea* scrollArea;     ///< Scrollable area for content

    // Folder Management
    QWidget* folderContainer;    ///< Container for folder widgets
    QVBoxLayout* folderLayout;   ///< Layout for folder widgets
    QMap<QString, FolderWidget*> folderMap; ///< Map of folder paths to FolderWidget instances

    // Single File Management
    QWidget* singleFileContainer; ///< Container for single file widgets
    QVBoxLayout* singleFileLayout; ///< Layout for single file widgets
    QMap<QString, FileWidget*> singleFileMap; ///< Map of file paths to FileWidget instances

    // Buttons
    QPushButton* selectFolderBtn; ///< Button to select a folder
    QPushButton* selectFilesBtn; ///< Button to select WAV files
    QPushButton* createFeatureBtn; ///< Button to create the feature

    // File Name Input
    QLabel* fileNameLabel;       ///< Label for file name input
    QLineEdit* fileNameInput;     ///< Input field for generated file name

    // Private Methods
    void setupUI();                           ///< Sets up the user interface
    void addFolder(const QString& folderPath); ///< Adds a folder and its WAV files
    void addSingleFile(const QString& filePath); ///< Adds a single WAV file

signals:
    void playRequested(const QString& filePath);
};

#endif // ADDSOUNDFEATUREWIDGET_H
