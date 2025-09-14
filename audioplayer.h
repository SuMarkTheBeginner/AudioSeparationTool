#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QWidget>
#ifdef HAS_MULTIMEDIA
#include <QMediaPlayer>
#include <QSlider>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

/**
 * @brief AudioPlayer widget for playing audio files with control bar.
 *
 * Provides play, pause, stop, and seek functionality with a progress slider.
 */
class AudioPlayer : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the AudioPlayer.
     * @param parent The parent widget (default is nullptr).
     */
    explicit AudioPlayer(QWidget* parent = nullptr);

    /**
     * @brief Plays the specified audio file.
     * @param filePath Path to the audio file.
     */
    void playAudio(const QString& filePath);

    /**
     * @brief Pauses the current playback.
     */
    void pauseAudio();

    /**
     * @brief Stops the current playback.
     */
    void stopAudio();

    /**
     * @brief Seeks to the specified position.
     * @param position Position in milliseconds.
     */
    void seekAudio(qint64 position);

private slots:
    /**
     * @brief Handles play/pause button click.
     */
    void onPlayPauseClicked();

    /**
     * @brief Handles stop button click.
     */
    void onStopClicked();

    /**
     * @brief Updates the progress slider when media position changes.
     * @param position Current position in milliseconds.
     */
    void onPositionChanged(qint64 position);

    /**
     * @brief Updates the duration when media is loaded.
     * @param duration Total duration in milliseconds.
     */
    void onDurationChanged(qint64 duration);

    /**
     * @brief Handles slider value change for seeking.
     * @param value New slider value.
     */
    void onSliderValueChanged(int value);

    /**
     * @brief Updates UI when playback state changes.
     * @param state New playback state.
     */
    void onStateChanged(QMediaPlayer::PlaybackState state);

    /**
     * @brief Handles media player errors.
     * @param error The error that occurred.
     * @param errorString Description of the error.
     */
    void onErrorOccurred(QMediaPlayer::Error error, const QString& errorString);

private:
    QMediaPlayer* mediaPlayer;    ///< Media player for audio playback
    QSlider* progressSlider;      ///< Slider for seeking
    QPushButton* playPauseBtn;    ///< Button for play/pause
    QPushButton* stopBtn;         ///< Button for stop
    QLabel* timeLabel;            ///< Label showing current/total time
    QHBoxLayout* layout;          ///< Horizontal layout for controls

    /**
     * @brief Formats time in milliseconds to string (mm:ss).
     * @param ms Time in milliseconds.
     * @return Formatted time string.
     */
    QString formatTime(qint64 ms) const;

    /**
     * @brief Sets up the user interface.
     */
    void setupUI();
};
#else
class AudioPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit AudioPlayer(QWidget* parent = nullptr) : QWidget(parent) {}
    void playAudio(const QString&) {}
    void pauseAudio() {}
    void stopAudio() {}
    void seekAudio(qint64) {}
};
#endif

#endif // AUDIOPLAYER_H
