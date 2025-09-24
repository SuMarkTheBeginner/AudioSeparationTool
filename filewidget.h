#ifndef FILEWIDGET_H
#define FILEWIDGET_H

#include <QFrame>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QToolButton>

/**
 * @brief Widget representing a single WAV file with a checkbox.
 *
 * This widget displays the file name and path, and includes a checkbox
 * for selection. Clicking the header toggles the checkbox.
 */
class FileWidget : public QFrame
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the FileWidget.
     * @param filePath The path to the WAV file.
     * @param parent The parent widget (default is nullptr).
     */
    explicit FileWidget(const QString& filePath, QWidget* parent = nullptr);

    /**
     * @brief Returns the file path associated with this widget.
     * @return The file path as a QString.
     */
    QString filePath() const { return m_filePath; }

    /**
     * @brief Returns the checkbox widget.
     * @return Pointer to the QCheckBox.
     */
    QCheckBox* checkBox() const { return m_checkBox; }


protected:
    /**
     * @brief Event filter to handle mouse clicks on the header widget.
     * Toggles the checkbox when the header is clicked.
     *
     * @param obj The object receiving the event.
     * @param event The event to filter.
     * @return true if the event is handled, false otherwise.
     */
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QString m_filePath;       ///< The file path of the WAV file
    QWidget* headerWidget;    ///< Header widget containing checkbox and file name
    QCheckBox* m_checkBox;    ///< Checkbox for selection
    QLabel* fileNameLabel;    ///< Label displaying the file name
    QLabel* filePathLabel;    ///< Label displaying the full file path
    QVBoxLayout* mainLayout;  ///< Main vertical layout
    QToolButton* playButton;  ///< Button to play the audio file

    /**
     * @brief Sets up the user interface components.
     */
    void setupUI();

signals:
    void fileRemoved(const QString& filePath);   ///< Signal emitted when the file is requested to be removed
    void playRequested(const QString& filePath); ///< Signal emitted when play is requested for the file
};

#endif // FILEWIDGET_H
