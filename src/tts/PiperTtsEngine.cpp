#include "tts/PiperTtsEngine.h"

#include <QtConcurrent>
#include <QAudioOutput>
#include <QDir>
#include <QMediaPlayer>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include "settings/AppSettings.h"

PiperTtsEngine::PiperTtsEngine(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(&m_synthesisWatcher, &QFutureWatcher<QString>::finished, this, [this]() {
        const QString outputFile = m_synthesisWatcher.result();
        if (outputFile.isEmpty()) {
            m_processing = false;
            emit playbackFailed(QStringLiteral("Failed to synthesize audio"));
            return;
        }

        playFile(outputFile);
    });

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia) {
            processNext();
        }
    });
}

void PiperTtsEngine::enqueueSentence(const QString &sentence)
{
    if (sentence.trimmed().isEmpty()) {
        return;
    }

    m_sentences.enqueue(sentence);
    if (!m_processing) {
        processNext();
    }
}

void PiperTtsEngine::clear()
{
    m_sentences.clear();
    if (m_player) {
        m_player->stop();
    }
    m_processing = false;
}

bool PiperTtsEngine::isSpeaking() const
{
    return m_processing || !m_sentences.isEmpty();
}

void PiperTtsEngine::processNext()
{
    if (m_sentences.isEmpty()) {
        m_processing = false;
        emit playbackFinished();
        return;
    }

    if (m_settings->piperExecutable().isEmpty() || m_settings->piperVoiceModel().isEmpty()) {
        m_sentences.clear();
        m_processing = false;
        emit playbackFailed(QStringLiteral("Piper executable or voice model is not configured"));
        return;
    }

    m_processing = true;
    emit playbackStarted();
    const QString sentence = m_sentences.dequeue();
    m_synthesisWatcher.setFuture(QtConcurrent::run([this, sentence]() {
        return synthesizeAndProcess(sentence);
    }));
}

QString PiperTtsEngine::synthesizeAndProcess(const QString &sentence)
{
    const auto tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempRoot);
    const QString rawPath = tempRoot + QStringLiteral("/jarvis_tts_raw.wav");
    const QString processedPath = tempRoot + QStringLiteral("/jarvis_tts_processed.wav");

    {
        QProcess process;
        process.start(m_settings->piperExecutable(), {
            QStringLiteral("--model"), m_settings->piperVoiceModel(),
            QStringLiteral("--output_file"), rawPath,
            QStringLiteral("--length_scale"), QString::number(1.0 / std::max(0.1, m_settings->voiceSpeed()), 'f', 2)
        });
        process.write(sentence.toUtf8());
        process.closeWriteChannel();
        process.waitForFinished(10000);
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            return {};
        }
    }

    if (m_settings->ffmpegExecutable().isEmpty()) {
        return rawPath;
    }

    const QString filter = QStringLiteral("asetrate=22050*%1,aresample=22050,equalizer=f=1200:t=q:w=1:g=2,aecho=0.8:0.9:20:0.25,acompressor")
                               .arg(QString::number(m_settings->voicePitch(), 'f', 2));

    QProcess process;
    process.start(m_settings->ffmpegExecutable(), {
        QStringLiteral("-y"),
        QStringLiteral("-i"), rawPath,
        QStringLiteral("-af"),
        filter,
        processedPath
    });
    process.waitForFinished(10000);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return rawPath;
    }

    return processedPath;
}

void PiperTtsEngine::playFile(const QString &path)
{
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();
}
