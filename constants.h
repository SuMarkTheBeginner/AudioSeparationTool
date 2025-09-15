#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

/**
 * @brief Namespace for application constants.
 */
namespace Constants {

// UI Strings
const QString APP_TITLE = "Qt MainWindow with Left Sidebar";
const QString HOW_TO_USE_TEXT = "This is the How to Use page.";
const QString SELECT_WAV_FOLDERS_TEXT = "Select folders containing WAV files or drag them here:";
const QString SELECT_WAV_FILES_TEXT = "Select WAV Files or Folders to Process:";
const QString OUTPUT_FILE_NAME_LABEL = "Output File Name:";
const QString FILE_NAME_PLACEHOLDER = "Enter file name...";
const QString CREATE_FEATURE_BUTTON = "Create Feature";
const QString PROCESS_BUTTON = "Process";
const QString SELECT_FEATURE_LABEL = "Select Sound Feature:";
const QString PROCESSED_FILES_LABEL = "Processed Files:";
const QString DELETE_BUTTON = "Delete";

// Error Messages
const QString FOLDER_NOT_EXIST = "Error: Folder does not exist: %1";
const QString NO_WAV_FILES_IN_FOLDER = "No WAV files found in folder: %1";
const QString FILE_NOT_EXIST = "Error: File does not exist: %1";
const QString FILE_NOT_READABLE = "Error: File is not readable: %1";
const QString FILE_NOT_WAV = "Error: File is not a WAV file: %1";
const QString FEATURE_NOT_SELECTED = "Please select a sound feature.";
const QString NO_FILES_SELECTED = "Please select at least one file to process.";
const QString OUTPUT_FEATURES_FOLDER_MISSING = "output_features folder does not exist.";
const QString DELETE_FEATURE_CONFIRM = "Are you sure you want to delete the feature '%1'?";
const QString NO_FEATURE_SELECTED_DELETE = "No feature selected to delete.";

// Tooltips
const QString PLAY_FILE_TOOLTIP = "播放此檔案";
const QString REMOVE_FILE_TOOLTIP = "移除此檔案";

// File Extensions
const QString WAV_EXTENSION = ".wav";
const QString TXT_EXTENSION = ".txt";

// UI Sizes
const int SIDEBAR_WIDTH = 200;
const int PROGRESS_BAR_HEIGHT = 18;
const int AUDIO_PLAYER_HEIGHT = 50;
const int BUTTON_SIZE = 30;
const int REMOVE_BUTTON_SIZE = 20;
const int SCROLL_AREA_MIN_HEIGHT = 300;
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// Other Constants
const int PROGRESS_RANGE_MIN = 0;
const int PROGRESS_RANGE_MAX = 100;

}

#endif // CONSTANTS_H
