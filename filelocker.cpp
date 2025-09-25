#include "filelocker.h"
#include "logger.h"

FileLocker::FileLocker(QObject* parent)
    : QObject(parent)
{
    LOG_DEBUG("FileLocker: Initialized file locker");
}

FileLocker::~FileLocker()
{
    LOG_DEBUG("FileLocker: Destroying file locker, unlocking all files");
    unlockAllFiles();
}

bool FileLocker::lockFile(const QString& path)
{
    // Check if already locked
    if (m_lockedFiles.contains(path)) {
        LOG_WARNING(QString("FileLocker: File already locked: %1").arg(path));
        return false;
    }

    // Attempt to set read-only
    auto result = FileUtils::setFileReadOnly(path, true);
    if (result == FileUtils::FileOperationResult::Success) {
        m_lockedFiles.insert(path);
        LOG_INFO(QString("FileLocker: Successfully locked file: %1").arg(path));
        emit fileLocked(path);
        return true;
    } else {
        LOG_ERROR(QString("FileLocker: Failed to lock file: %1 (error: %2)")
                 .arg(path).arg(static_cast<int>(result)));
        return false;
    }
}

bool FileLocker::unlockFile(const QString& path)
{
    // Check if actually locked
    if (!m_lockedFiles.contains(path)) {
        LOG_WARNING(QString("FileLocker: File not locked, cannot unlock: %1").arg(path));
        return false;
    }

    // Attempt to make writable
    auto result = FileUtils::setFileReadOnly(path, false);
    if (result == FileUtils::FileOperationResult::Success) {
        m_lockedFiles.remove(path);
        LOG_INFO(QString("FileLocker: Successfully unlocked file: %1").arg(path));
        emit fileUnlocked(path);
        return true;
    } else {
        LOG_ERROR(QString("FileLocker: Failed to unlock file: %1 (error: %2)")
                 .arg(path).arg(static_cast<int>(result)));
        return false;
    }
}

bool FileLocker::isFileLocked(const QString& path) const
{
    return m_lockedFiles.contains(path);
}

QStringList FileLocker::getLockedFiles() const
{
    QStringList locked;
    for (const QString& path : m_lockedFiles) {
        locked << path;
    }
    return locked;
}

void FileLocker::unlockAllFiles()
{
    LOG_INFO(QString("FileLocker: Unlocking all %1 files").arg(m_lockedFiles.size()));

    // Create a copy of the set for iteration since we'll be modifying it
    QSet<QString> toUnlock = m_lockedFiles;

    for (const QString& path : toUnlock) {
        if (!unlockFile(path)) {
            LOG_ERROR(QString("FileLocker: Failed to unlock file during cleanup: %1").arg(path));
        }
    }

    if (!m_lockedFiles.isEmpty()) {
        LOG_WARNING(QString("FileLocker: %1 files still locked after cleanup")
                   .arg(m_lockedFiles.size()));
    }
}
