#ifndef FILELOCKER_H
#define FILELOCKER_H

#include <QObject>
#include <QSet>
#include <QString>
#include "fileutils.h"

/**
 * @brief Manages file locking operations to prevent concurrent modifications.
 *
 * Provides a centralized way to lock and unlock files using read-only attributes
 * to indicate processing state. This prevents multiple operations from
 * modifying the same files simultaneously.
 */
class FileLocker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor.
     * @param parent Optional parent QObject.
     */
    explicit FileLocker(QObject* parent = nullptr);

    /**
     * @brief Destructor - unlocks all locked files.
     */
    ~FileLocker();

    /**
     * @brief Attempt to lock a file.
     * @param path Absolute path to the file to lock.
     * @return True if successfully locked, false if already locked or operation failed.
     */
    bool lockFile(const QString& path);

    /**
     * @brief Unlock a previously locked file.
     * @param path Absolute path to the file to unlock.
     * @return True if successfully unlocked, false if not locked or operation failed.
     */
    bool unlockFile(const QString& path);

    /**
     * @brief Check if a file is currently locked.
     * @param path Absolute path to the file to check.
     * @return True if the file is locked, false otherwise.
     */
    bool isFileLocked(const QString& path) const;

    /**
     * @brief Get a list of all currently locked files.
     * @return QList of absolute file paths that are locked.
     */
    QStringList getLockedFiles() const;

    /**
     * @brief Unlock all currently locked files.
     *
     * Typically called during cleanup or emergency shutdown.
     */
    void unlockAllFiles();

signals:
    void fileLocked(const QString& path);
    void fileUnlocked(const QString& path);

private:
    QSet<QString> m_lockedFiles;
};

#endif // FILELOCKER_H
