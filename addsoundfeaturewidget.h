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
#include "filemanagerwidget.h"

/**
 * @brief Widget for adding sound features by selecting folders or dragging WAV files.
 *
 * This widget allows users to select folders containing WAV files or drag individual
 * WAV files into the interface. It organizes them into FolderWidget and FileWidget
 * components, provides sorting options, and supports drag-and-drop functionality.
 */
class AddSoundFeatureWidget : public FileManagerWidget
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

private:
    // File Name Input
    QLabel* fileNameLabel;       ///< Label for file name input
    QLineEdit* fileNameInput;     ///< Input field for generated file name
    QPushButton* createFeatureBtn; ///< Button to create the feature

    // Private Methods
    void setupFeatureInputUI();               ///< Sets up the feature input UI components
    void setupFeatureButtonConnections();     ///< Sets up the connections for the feature creation button
};

#endif // ADDSOUNDFEATUREWIDGET_H
