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

// New Constants for paths
const QString OUTPUT_FEATURES_DIR = "output_features";       // Sound feature embeddings
const QString SEPARATED_RESULT_DIR = "separated_results";     // Separation results
const QString TEMP_SEGMENTS_DIR = "temp_chunks";             // Temporary chunks during processing

// Model file paths (absolute paths for development)
const QString HTSAT_MODEL_PATH = "C:/Users/aaa09/Desktop/Project/AudioSeparationTool_Windows/AudioSeparationTool/models/htsat_embedding_model.pt";              // HTSAT model folder
const QString ZERO_SHOT_ASP_MODEL_PATH = "C:/Users/aaa09/Desktop/Project/AudioSeparationTool_Windows/AudioSeparationTool/models/zero_shot_asp_separation_model.pt"; // ZeroShotASP model file

// Model resource paths (for embedded models)
const QString HTSAT_MODEL_RESOURCE = ":/models/htsat_embedding_model.pt";              // HTSAT model resource path
const QString ZERO_SHOT_ASP_MODEL_RESOURCE = ":/models/zero_shot_asp_separation_model.pt"; // ZeroShotASP model resource path

// Audio processing constants
const int AUDIO_SAMPLE_RATE = 32000;        // Sample rate in Hz
const int AUDIO_CLIP_SAMPLES = 320000;      // Number of samples per clip (10 seconds @ 32kHz)
const float AUDIO_OVERLAP_RATE = 0.5f;      // Overlap rate for overlap-add processing

// Debug announcement constants
const QString DEBUG_FILE_SELECTED = "Debug: File selected - %1";
const QString DEBUG_FILE_DESELECTED = "Debug: File deselected - %1";
const QString DEBUG_PLAY_CLICKED = "Debug: Play button clicked for file - %1";
const QString DEBUG_REMOVE_CLICKED = "Debug: Remove button clicked for file - %1";
const QString DEBUG_PROCESS_CLICKED = "Debug: Process button clicked with %1 files";
const QString DEBUG_FEATURE_SELECTED = "Debug: Feature selected - %1";
const QString DEBUG_FILE_TARGET = "Debug: Input target file - %1";
const QString DEBUG_PROCESSING_STARTED = "Debug: Processing started for %1 files";
const QString DEBUG_PROCESSING_PROGRESS = "Debug: Processing progress - %1%";
const QString DEBUG_PROCESSING_FINISHED = "Debug: Processing finished with %1 results";
const QString DEBUG_FEATURE_CREATED = "Debug: Feature creation started for %1 files";
const QString DEBUG_FOLDER_ADDED = "Debug: Folder added - %1";
const QString DEBUG_FILE_ADDED = "Debug: File added - %1";



}

#endif // CONSTANTS_H
