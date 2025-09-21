#include "fileutils.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QDir>
#include <QThread>
#include <QCoreApplication>
#include <QProcess>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace FileUtils {

FileOperationResult setFileReadOnly(const QString& path, bool readOnly,
                                   QString* errorMessage, int maxRetries, int retryDelayMs)
{
    QFileInfo info(path);

    // Check if file exists
    if (!info.exists()) {
        if (errorMessage) {
            *errorMessage = QString("File does not exist: %1").arg(path);
        }
        return FileOperationResult::FileNotFound;
    }

    if (!info.isFile()) {
        if (errorMessage) {
            *errorMessage = QString("Path is not a file: %1").arg(path);
        }
        return FileOperationResult::InvalidPath;
    }

    // Check if file is locked
    if (isFileLocked(path)) {
        if (errorMessage) {
            *errorMessage = QString("File is locked by another process: %1").arg(path);
        }
        return FileOperationResult::FileLocked;
    }

    // Detect filesystem type
    FilesystemType fsType = detectFilesystemType(path);

    // Execute with retry mechanism
    for (int attempt = 0; attempt <= maxRetries; ++attempt) {
        FileOperationResult result = executeSetFileReadOnly(path, readOnly, fsType, errorMessage);

        if (result == FileOperationResult::Success) {
            qDebug() << "Set read-only" << (readOnly ? "on" : "off") << "for:" << path
                     << "(attempt" << (attempt + 1) << "of" << (maxRetries + 1) << ")";
            return result;
        }

        // If not the last attempt, wait before retrying
        if (attempt < maxRetries) {
            qDebug() << "Retrying setFileReadOnly for:" << path
                     << "after" << retryDelayMs << "ms (attempt" << (attempt + 1) << ")";
            QThread::msleep(retryDelayMs);
        }
    }

    if (errorMessage) {
        *errorMessage = QString("Failed to set read-only %1 for %2 after %3 attempts")
                      .arg(readOnly ? "on" : "off").arg(path).arg(maxRetries + 1);
    }
    return FileOperationResult::RetryFailed;
}

bool isFileLocked(const QString& path)
{
    QFile file(path);

    // Try to open the file in read-only mode
    if (file.open(QIODevice::ReadOnly)) {
        file.close();
        return false; // File is not locked
    }

    // Check if it's a permission issue rather than locking
    QFileInfo info(path);
    if (!info.isWritable()) {
        return false; // File is read-only but not necessarily locked
    }

    return true; // File is likely locked by another process
}

FilesystemType detectFilesystemType(const QString& path)
{
#ifdef _WIN32
    return FilesystemType::WindowsNative;
#else
    // Check if we're running under WSL
    QFileInfo wslCheck("/proc/version");
    if (wslCheck.exists()) {
        QFile file("/proc/version");
        if (file.open(QIODevice::ReadOnly)) {
            QString version = file.readAll();
            file.close();
            if (version.contains("Microsoft") || version.contains("WSL")) {
                return FilesystemType::WSL_NTFS;
            }
        }
    }

    return FilesystemType::LinuxNative;
#endif
}

QString convertWSLPathToWindows(const QString& wslPath)
{
    QString result = wslPath;

    // Convert /mnt/c/ to C:\, /mnt/d/ to D:\, etc.
    if (result.startsWith("/mnt/c/")) {
        result.replace(0, 7, "C:\\");
    } else if (result.startsWith("/mnt/d/")) {
        result.replace(0, 7, "D:\\");
    } else if (result.startsWith("/mnt/e/")) {
        result.replace(0, 7, "E:\\");
    } else if (result.startsWith("/mnt/f/")) {
        result.replace(0, 7, "F:\\");
    }

    // Convert forward slashes to backslashes
    result.replace("/", "\\");

    return result;
}

FileOperationResult executeSetFileReadOnly(const QString& path, bool readOnly,
                                          FilesystemType fsType, QString* errorMessage)
{
    switch (fsType) {
        case FilesystemType::WindowsNative:
            return setFileReadOnlyWindows(path, readOnly, errorMessage);
        case FilesystemType::LinuxNative:
            return setFileReadOnlyLinux(path, readOnly, errorMessage);
        case FilesystemType::WSL_NTFS:
            return setFileReadOnlyWSL(path, readOnly, errorMessage);
        default:
            if (errorMessage) {
                *errorMessage = QString("Unsupported filesystem type for: %1").arg(path);
            }
            return FileOperationResult::UnsupportedFilesystem;
    }
}

FileOperationResult setFileReadOnlyWindows(const QString& path, bool readOnly,
                                          QString* errorMessage)
{
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW((LPCWSTR)path.utf16());

    if (attrs == INVALID_FILE_ATTRIBUTES) {
        DWORD errorCode = GetLastError();
        if (errorMessage) {
            *errorMessage = QString("GetFileAttributes failed for %1 (Windows error: %2)")
                          .arg(path).arg(errorCode);
        }
        return FileOperationResult::SystemError;
    }

    if (readOnly) {
        attrs |= FILE_ATTRIBUTE_READONLY;
    } else {
        attrs &= ~FILE_ATTRIBUTE_READONLY;
    }

    if (!SetFileAttributesW((LPCWSTR)path.utf16(), attrs)) {
        DWORD errorCode = GetLastError();
        if (errorMessage) {
            *errorMessage = QString("SetFileAttributes failed for %1 (Windows error: %2)")
                          .arg(path).arg(errorCode);
        }
        return FileOperationResult::PermissionDenied;
    }

    return FileOperationResult::Success;
#else
    if (errorMessage) {
        *errorMessage = QString("Windows function called on non-Windows system for: %1").arg(path);
    }
    return FileOperationResult::SystemError;
#endif
}

FileOperationResult setFileReadOnlyLinux(const QString& path, bool readOnly,
                                        QString* errorMessage)
{
#ifndef _WIN32
    struct stat fileStat;
    if (stat(path.toUtf8().constData(), &fileStat) != 0) {
        if (errorMessage) {
            *errorMessage = QString("stat failed for %1 (errno: %2 - %3)")
                          .arg(path).arg(errno).arg(strerror(errno));
        }
        return FileOperationResult::SystemError;
    }

    mode_t mode = fileStat.st_mode;

    if (readOnly) {
        // Remove write permissions for user, group, and others
        mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    } else {
        // Restore basic write permission for user
        mode |= S_IWUSR;
    }

    if (chmod(path.toUtf8().constData(), mode) != 0) {
        if (errorMessage) {
            *errorMessage = QString("chmod failed for %1 (errno: %2 - %3)")
                          .arg(path).arg(errno).arg(strerror(errno));
        }
        return FileOperationResult::PermissionDenied;
    }

    return FileOperationResult::Success;
#else
    if (errorMessage) {
        *errorMessage = QString("Linux function called on non-Linux system for: %1").arg(path);
    }
    return FileOperationResult::SystemError;
#endif
}

FileOperationResult setFileReadOnlyWSL(const QString& path, bool readOnly,
                                      QString* errorMessage)
{
#ifndef _WIN32
    QProcess process;
    QStringList args;

    // Use cmd.exe through winexe to run attrib command
    args << "/c" << "attrib" << (readOnly ? "+R" : "-R") << convertWSLPathToWindows(path);

    process.start("cmd.exe", args);
    if (!process.waitForFinished(5000)) { // 5 second timeout
        if (errorMessage) {
            *errorMessage = QString("WSL attrib command timed out for: %1").arg(path);
        }
        return FileOperationResult::SystemError;
    }

    int exitCode = process.exitCode();
    if (exitCode != 0) {
        QString errorOutput = process.readAllStandardError();
        if (errorMessage) {
            *errorMessage = QString("WSL attrib command failed for %1 (exit code: %2, error: %3)")
                          .arg(path).arg(exitCode).arg(errorOutput.trimmed());
        }
        return FileOperationResult::PermissionDenied;
    }

    return FileOperationResult::Success;
#else
    if (errorMessage) {
        *errorMessage = QString("WSL function called on non-WSL system for: %1").arg(path);
    }
    return FileOperationResult::SystemError;
#endif
}

} // namespace FileUtils
