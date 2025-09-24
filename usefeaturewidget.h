#ifndef USEFEATUREWIDGET_H
#define USEFEATUREWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QDir>
#include <QFileInfoList>
#include "folderwidget.h"
#include "filewidget.h"
#include "resourcemanager.h"
#include "filemanagerwidget.h"

/**
 * @brief Widget for using existing sound features to process WAV files.
 *
 * This widget allows users to select a sound feature from the output_features folder,
 * choose WAV files or folders to process, initiate processing, and play the results.
 */
class UseFeatureWidget : public FileManagerWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the UseFeatureWidget.
     * @param parent The parent widget (default is nullptr).
     */
    explicit UseFeatureWidget(QWidget *parent = nullptr);

private:
    // UI Components
    QLabel* featureLabel;             ///< Label for feature selection
    QComboBox* featureComboBox;       ///< Combo box to select sound feature
    QPushButton* processButton;       ///< Button to start processing
    QLabel* resultLabel;              ///< Label for results
    QListWidget* resultList;          ///< List widget for processed files with play buttons

    // Private Methods
    void setupUI();                          ///< Sets up the user interface
    void loadFeatures();                      ///< Loads available features from output_features folder
    void addFolder(const QString& folderPath); ///< Adds a folder and its WAV files
    void addSingleFile(const QString& filePath); ///< Adds a single WAV file
    QStringList splitAndSaveTempFiles(const QString& audioPath); ///< Splits audio into chunks and saves temp files with padding
    void setupFeatureSelectionUI();           ///< Sets up the feature selection UI components
    void setupProcessingUI();                   ///< Sets up the processing UI components
    void setupConnections();                    ///< Sets up the signal-slot connections


public slots:
    void refreshFeatures();                   ///< Refreshes the feature list

private slots:
    void onProcessClicked();                  ///< Handles process button click
    void onDeleteClicked();                   ///< Handles delete button click
    void onProcessingProgress(int value);     ///< Handles processing progress updates
    void onProcessingFinished(const QStringList& results); ///< Handles processing completion
    void onProcessingError(const QString& error); ///< Handles processing error

signals:
    void playRequested(const QString& filePath); ///< Signal emitted when play is requested

private slots:
    void onSeparationProcessingFinished(const QStringList& results); ///< Handles separation processing completion
};


#endif // USEFEATUREWIDGET_H
