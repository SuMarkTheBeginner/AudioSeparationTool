#ifndef FOLDERWIDGET_H
#define FOLDERWIDGET_H

#include <QFrame>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QList>
#include <QSet>
#include <QPushButton>

#include "widecheckbox.h"
/**
 * @brief Widget representing a folder containing WAV files.
 *
 * This widget displays the folder name with a checkbox and an expandable list
 * of file checkboxes. It manages the selection state of the folder based on
 * the state of its files and supports toggling visibility of the file list.
 */
class FolderWidget : public QFrame
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the FolderWidget.
     * @param folderPath The path to the folder.
     * @param parent The parent widget (default is nullptr).
     */
    explicit FolderWidget(const QString& folderPath, QWidget* parent = nullptr);

    /**
     * @brief Returns the folder path associated with this widget.
     * @return The folder path as a QString.
     */
    QString folderPath() const { return m_folderPath; }

    /**
     * @brief Appends files to the folder widget.
     * @param files List of file names to add.
     */
    void appendFiles(const QStringList& files);

protected:
    /**
     * @brief Event filter to handle mouse events on header and labels.
     *
     * Toggles the file list visibility when the header or labels are clicked.
     *
     * @param obj The object receiving the event.
     * @param event The event to filter.
     * @return true if the event is handled, false otherwise.
     */
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QString m_folderPath;           ///< The folder path

    // Header
    QWidget* headerWidget;          ///< Header widget containing checkbox and folder name
    QCheckBox* folderCheckBox;      ///< Checkbox for folder selection
    QLabel* folderNameLabel;        ///< Label displaying the folder name
    QLabel* arrowLabel;             ///< Label displaying the expand/collapse arrow
    QPushButton* removeFolderBtn;   ///< Button to remove the entire folder
    QVBoxLayout* mainLayout;        ///< Main vertical layout

    // Files
    QWidget* filesContainer;        ///< Container widget for file checkboxes
    QVBoxLayout* filesLayout;       ///< Layout for file checkboxes
    QList<WideCheckBox*> filesCheckBoxes; ///< List of file checkboxes
    QSet<QString> addedFiles;       ///< Set of added file paths to prevent duplicates

    WideCheckBox* findCheckBoxByText(const QString& text) const;

    /**
     * @brief Sets up the user interface components.
     */
    void setupUI();

    /**
     * @brief Adds files internally to the widget.
     * @param files List of file names to add.
     */
    void addFilesInternal(const QStringList& files);

    /**
     * @brief Refreshes the folder checkbox state based on file checkbox states.
     */
    void refreshFolderCheckState();

    /**
     * @brief Toggles the visibility of the file list.
     */
    void toggleFilesVisible();


signals:
    void fileRemoved(const QString& filePath);
    void folderRemoved(const QString& folderPath);

};

#endif // FOLDERWIDGET_H
