// FileUtils.h
#pragma once
#include <QString>
#include <QProcess>

/**
 * @brief Namespace for file utility functions.
 */
namespace FileUtils {

/**
 * @brief Result codes for file operations.
 */
enum class FileOperationResult {
    Success,
    FileNotFound,
    PermissionDenied,
    FileLocked,
    InvalidPath,
    SystemError,
    UnsupportedFilesystem,
    RetryFailed
};

/**
 * @brief Filesystem types for cross-platform compatibility.
 */
enum class FilesystemType {
    WindowsNative,
    LinuxNative,
    WSL_NTFS,
    Unknown
};

/**
 * @brief Sets the read-only attribute of a file with enhanced cross-platform support.
 * @param path The path to the file.
 * @param readOnly True to set read-only, false to remove read-only.
 * @param errorMessage Optional pointer to store detailed error message.
 * @param maxRetries Maximum number of retry attempts for temporary failures (default: 3).
 * @param retryDelayMs Delay between retries in milliseconds (default: 100).
 * @return FileOperationResult indicating success or specific error type.
 */
FileUtils::FileOperationResult setFileReadOnly(const QString& path, bool readOnly,
                                               QString* errorMessage = nullptr,
                                               int maxRetries = 3, int retryDelayMs = 100);

/**
 * @brief Checks if a file is currently locked by another process.
 * @param path The path to the file.
 * @return True if file is locked, false otherwise.
 */
bool isFileLocked(const QString& path);

/**
 * @brief Detects the filesystem type for the given path.
 * @param path The path to analyze.
 * @return FilesystemType indicating the detected filesystem.
 */
FilesystemType detectFilesystemType(const QString& path);

/**
 * @brief Converts a WSL path to Windows format.
 * @param wslPath WSL path (e.g., "/mnt/c/Users/file.txt").
 * @return Windows path (e.g., "C:\Users\file.txt").
 */
QString convertWSLPathToWindows(const QString& wslPath);

/**
 * @brief Executes the actual file read-only setting based on filesystem type.
 * @param path The path to the file.
 * @param readOnly True to set read-only, false to remove read-only.
 * @param fsType The detected filesystem type.
 * @param errorMessage Optional pointer to store detailed error message.
 * @return FileOperationResult indicating success or specific error type.
 */
FileUtils::FileOperationResult executeSetFileReadOnly(const QString& path, bool readOnly,
                                                     FilesystemType fsType, QString* errorMessage = nullptr);

/**
 * @brief Windows native implementation using SetFileAttributesW.
 */
FileUtils::FileOperationResult setFileReadOnlyWindows(const QString& path, bool readOnly,
                                                     QString* errorMessage = nullptr);

/**
 * @brief Linux native implementation using chmod.
 */
FileUtils::FileOperationResult setFileReadOnlyLinux(const QString& path, bool readOnly,
                                                   QString* errorMessage = nullptr);

/**
 * @brief WSL NTFS implementation using cmd.exe attrib command.
 */
FileUtils::FileOperationResult setFileReadOnlyWSL(const QString& path, bool readOnly,
                                                 QString* errorMessage = nullptr);

}
