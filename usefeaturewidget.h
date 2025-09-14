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

/**
 * @brief Widget for using existing sound features to process WAV files.
 *
 * This widget allows users to select a sound feature from the output_features folder,
 * choose WAV files or folders to process, initiate processing, and play the results.
 */
class UseFeatureWidget : public QWidget
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
    QVBoxLayout* mainLayout;          ///< Main vertical layout
    QLabel* featureLabel;             ///< Label for feature selection
    QComboBox* featureComboBox;       ///< Combo box to select sound feature
    QLabel* fileLabel;                ///< Label for file selection
    QLabel* statusLabel;              ///< Label for status messages
    QScrollArea* fileScrollArea;      ///< Scrollable area for file selection
    QWidget* fileContainer;           ///< Container for file widgets
    QVBoxLayout* fileLayout;          ///< Layout for file widgets
    QPushButton* processButton;       ///< Button to start processing
    QLabel* resultLabel;              ///< Label for results
    QListWidget* resultList;          ///< List widget for processed files with play buttons

    // File Management (now using ResourceManager)

    // Private Methods
    void setupUI();                           ///< Sets up the user interface
    void loadFeatures();                      ///< Loads available features from output_features folder
    void addFolder(const QString& folderPath); ///< Adds a folder and its WAV files
    void addSingleFile(const QString& filePath); ///< Adds a single WAV file
    QStringList splitAndSaveTempFiles(const QString& audioPath); ///< Splits audio into chunks and saves temp files with padding


public slots:
    void refreshFeatures();                   ///< Refreshes the feature list

private slots:
    void onProcessClicked();                  ///< Handles process button click
    void onPlayResult(const QString& filePath); ///< Handles play request for result files
    void onDeleteClicked();                   ///< Handles delete button click

signals:
    void playRequested(const QString& filePath); ///< Signal emitted when play is requested
};


#endif // USEFEATUREWIDGET_H
