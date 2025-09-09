// FileUtils.h
#pragma once
#include <QString>

/**
 * @brief Namespace for file utility functions.
 */
namespace FileUtils {
/**
     * @brief Sets the read-only attribute of a file.
     * @param path The path to the file.
     * @param readOnly True to set read-only, false to remove read-only.
     * @return True if successful, false otherwise.
     */
bool setFileReadOnly(const QString& path, bool readOnly);
}
