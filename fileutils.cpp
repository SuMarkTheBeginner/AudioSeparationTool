// FileUtils.cpp
#include "fileutils.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

/**
 * @brief Sets the read-only attribute of a file.
 * @param path The path to the file.
 * @param readOnly True to set read-only, false to remove read-only.
 * @return True if successful, false otherwise.
 */
bool FileUtils::setFileReadOnly(const QString& path, bool readOnly) {
    QFileInfo info(path);
    if (!info.exists()) {
        qDebug() << "File does not exist:" << path;
        return false;
    }
    if (!info.isFile()) {
        qDebug() << "Path is not a file:" << path;
        return false;
    }

#ifdef _WIN32
    DWORD attrs = GetFileAttributesW((LPCWSTR)path.utf16());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        qDebug() << "GetFileAttributes failed for:" << path;
        return false;
    }
    if (readOnly)
        attrs |= FILE_ATTRIBUTE_READONLY;
    else
        attrs &= ~FILE_ATTRIBUTE_READONLY;

    if (!SetFileAttributesW((LPCWSTR)path.utf16(), attrs)) {
        qDebug() << "SetFileAttributes failed for:" << path;
        return false;
    }
    qDebug() << "Set read-only" << (readOnly ? "on" : "off") << "for:" << path;
    return true;
#else
    mode_t mode = info.permissions();
    if (readOnly) {
        mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    } else {
        mode |= (S_IWUSR); // Restore basic write permission
    }
    if (chmod(path.toUtf8().constData(), mode) != 0) {
        qDebug() << "chmod failed for:" << path;
        return false;
    }
    qDebug() << "Set read-only" << (readOnly ? "on" : "off") << "for:" << path;
    return true;
#endif
}
