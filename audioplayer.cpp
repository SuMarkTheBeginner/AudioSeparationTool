#include "audioplayer.h"
#include <QMessageBox>
#ifdef HAS_MULTIMEDIA
#include <QUrl>

/**
 * @brief Constructs the AudioPlayer.
 * @param parent The parent widget.
 */
AudioPlayer::AudioPlayer(QWidget* parent)
    : QWidget(parent), mediaPlayer(new QMediaPlayer(this))
{
    setupUI();

    // Connect media player signals
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, &AudioPlayer::onPositionChanged);
    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, &AudioPlayer::onDurationChanged);
    connect(mediaPlayer, QOverload<QMediaPlayer::PlaybackState>::of(&QMediaPlayer::playbackStateChanged),
            this, &AudioPlayer::onStateChanged);
    connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, &AudioPlayer::onErrorOccurred);

    // Connect UI signals
    connect(playPauseBtn, &QPushButton::clicked, this, &AudioPlayer::onPlayPauseClicked);
    connect(stopBtn, &QPushButton::clicked, this, &AudioPlayer::onStopClicked);
    connect(progressSlider, &QSlider::sliderMoved, this, &AudioPlayer::onSliderValueChanged);
}

/**
 * @brief Sets up the user interface.
 */
void AudioPlayer::setupUI()
{
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(10);

    playPauseBtn = new QPushButton("▶", this);
    playPauseBtn->setFixedSize(30, 30);
    layout->addWidget(playPauseBtn);

    stopBtn = new QPushButton("⏹", this);
    stopBtn->setFixedSize(30, 30);
    layout->addWidget(stopBtn);

    progressSlider = new QSlider(Qt::Horizontal, this);
    progressSlider->setRange(0, 100);
    layout->addWidget(progressSlider, 1);

    timeLabel = new QLabel("00:00 / 00:00", this);
    layout->addWidget(timeLabel);

    setLayout(layout);
    setFixedHeight(50);
}

/**
 * @brief Plays the specified audio file.
 * @param filePath Path to the audio file.
 */
void AudioPlayer::playAudio(const QString& filePath)
{
#ifdef HAS_MULTIMEDIA
    mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
    mediaPlayer->play();
#else
    // Show a message when multimedia is not available
    QMessageBox::information(this, "Audio Playback Not Available",
        "Audio playback is not available because Qt Multimedia is not installed or not found.\n\n"
        "To enable audio playback, please install Qt Multimedia:\n"
        "  - On Ubuntu/Debian: sudo apt-get install qtmultimedia5-dev\n"
        "  - On Fedora/CentOS: sudo dnf install qt5-qtmultimedia-devel\n"
        "  - On macOS: brew install qt@5\n"
        "Then rebuild the application.");
#endif
}

/**
 * @brief Pauses the current playback.
 */
void AudioPlayer::pauseAudio()
{
    mediaPlayer->pause();
}

/**
 * @brief Stops the current playback.
 */
void AudioPlayer::stopAudio()
{
    mediaPlayer->stop();
}

/**
 * @brief Seeks to the specified position.
 * @param position Position in milliseconds.
 */
void AudioPlayer::seekAudio(qint64 position)
{
    mediaPlayer->setPosition(position);
}

/**
 * @brief Handles play/pause button click.
 */
void AudioPlayer::onPlayPauseClicked()
{
    if (mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        mediaPlayer->pause();
    } else {
        mediaPlayer->play();
    }
}

/**
 * @brief Handles stop button click.
 */
void AudioPlayer::onStopClicked()
{
    mediaPlayer->stop();
}

/**
 * @brief Updates the progress slider when media position changes.
 * @param position Current position in milliseconds.
 */
void AudioPlayer::onPositionChanged(qint64 position)
{
    if (mediaPlayer->duration() > 0) {
        int sliderValue = static_cast<int>((position * 100) / mediaPlayer->duration());
        progressSlider->blockSignals(true);
        progressSlider->setValue(sliderValue);
        progressSlider->blockSignals(false);

        timeLabel->setText(formatTime(position) + " / " + formatTime(mediaPlayer->duration()));
    }
}

/**
 * @brief Updates the duration when media is loaded.
 * @param duration Total duration in milliseconds.
 */
void AudioPlayer::onDurationChanged(qint64 duration)
{
    progressSlider->setRange(0, 100);
    timeLabel->setText("00:00 / " + formatTime(duration));
}

/**
 * @brief Handles slider value change for seeking.
 * @param value New slider value.
 */
void AudioPlayer::onSliderValueChanged(int value)
{
    if (mediaPlayer->duration() > 0) {
        qint64 position = (value * mediaPlayer->duration()) / 100;
        mediaPlayer->setPosition(position);
    }
}

/**
 * @brief Updates UI when playback state changes.
 * @param state New playback state.
 */
void AudioPlayer::onStateChanged(QMediaPlayer::PlaybackState state)
{
    switch (state) {
    case QMediaPlayer::PlayingState:
        playPauseBtn->setText("⏸");
        break;
    case QMediaPlayer::PausedState:
    case QMediaPlayer::StoppedState:
        playPauseBtn->setText("▶");
        break;
    }
}

/**
 * @brief Handles media player errors.
 * @param error The error that occurred.
 * @param errorString Description of the error.
 */
void AudioPlayer::onErrorOccurred(QMediaPlayer::Error error, const QString& errorString)
{
    QString errorMessage;
    switch (error) {
    case QMediaPlayer::NoError:
        return; // No error, nothing to do
    case QMediaPlayer::ResourceError:
        errorMessage = "Resource error: " + errorString + "\n\n"
                      "This usually means the audio file could not be loaded or accessed.";
        break;
    case QMediaPlayer::FormatError:
        errorMessage = "Format error: " + errorString + "\n\n"
                      "This usually means the audio format is not supported. "
                      "For WAV files, ensure you have the necessary multimedia codecs installed.\n\n"
                      "On Ubuntu/Debian: sudo apt-get install libqt5multimedia5-plugins\n"
                      "On Fedora/CentOS: sudo dnf install qt5-qtmultimedia\n"
                      "On Windows: Qt Multimedia plugins should be included with Qt installation.\n\n"
                      "Note: If you're running in WSL (Windows Subsystem for Linux), audio playback "
                      "may not work due to limited multimedia support. Consider:\n"
                      "1. Running the application natively on Windows\n"
                      "2. Using X11 forwarding with audio support\n"
                      "3. Installing PulseAudio in WSL: sudo apt-get install pulseaudio";
        break;
    case QMediaPlayer::NetworkError:
        errorMessage = "Network error: " + errorString;
        break;
    case QMediaPlayer::AccessDeniedError:
        errorMessage = "Access denied: " + errorString + "\n\n"
                      "Check file permissions and ensure the audio file is readable.";
        break;
    default:
        errorMessage = "Playback error: " + errorString + "\n\n"
                      "This could be due to missing multimedia codecs or plugins. "
                      "For WAV files, ensure you have the necessary multimedia codecs installed.\n\n"
                      "On Ubuntu/Debian: sudo apt-get install libqt5multimedia5-plugins\n"
                      "On Fedora/CentOS: sudo dnf install qt5-qtmultimedia\n"
                      "On Windows: Qt Multimedia plugins should be included with Qt installation.\n\n"
                      "If you're running in WSL (Windows Subsystem for Linux):\n"
                      "- Audio playback has limited support in WSL\n"
                      "- Consider running the application natively on Windows\n"
                      "- Or use X11 forwarding with proper audio configuration";
        break;
    }

    QMessageBox::warning(this, "Audio Playback Error", errorMessage);
}

/**
 * @brief Formats time in milliseconds to string (mm:ss).
 * @param ms Time in milliseconds.
 * @return Formatted time string.
 */
QString AudioPlayer::formatTime(qint64 ms) const
{
    int seconds = static_cast<int>(ms / 1000);
    int minutes = seconds / 60;
    seconds %= 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}
#else
AudioPlayer::AudioPlayer(QWidget* parent) : QWidget(parent) {}
#endif
