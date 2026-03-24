#include "stt/WhisperSttEngine.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>

#include "settings/AppSettings.h"

namespace {
void writeWaveHeader(QFile &file, quint32 pcmSize)
{
    const quint32 sampleRate = 16000;
    const quint16 channels = 1;
    const quint16 bitsPerSample = 16;
    const quint32 byteRate = sampleRate * channels * bitsPerSample / 8;
    const quint16 blockAlign = channels * bitsPerSample / 8;
    const quint32 chunkSize = 36 + pcmSize;
    const quint32 subChunkSize = 16;
    const quint16 audioFormat = 1;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char *>(&chunkSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    file.write(reinterpret_cast<const char *>(&subChunkSize), 4);
    file.write(reinterpret_cast<const char *>(&audioFormat), 2);
    file.write(reinterpret_cast<const char *>(&channels), 2);
    file.write(reinterpret_cast<const char *>(&sampleRate), 4);
    file.write(reinterpret_cast<const char *>(&byteRate), 4);
    file.write(reinterpret_cast<const char *>(&blockAlign), 2);
    file.write(reinterpret_cast<const char *>(&bitsPerSample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char *>(&pcmSize), 4);
}
}

WhisperSttEngine::WhisperSttEngine(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

void WhisperSttEngine::transcribePcm(const QByteArray &pcmData)
{
    if (m_settings->whisperExecutable().isEmpty()) {
        emit transcriptionFailed(QStringLiteral("whisper.cpp executable is not configured"));
        return;
    }

    const QString waveFile = writeWaveFile(pcmData);
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        const auto cleanup = qScopeGuard([process]() { process->deleteLater(); });
        if (status != QProcess::NormalExit || exitCode != 0) {
            emit transcriptionFailed(QStringLiteral("whisper.cpp failed to transcribe input"));
            return;
        }

        const QString text = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        emit transcriptionReady({text, text.isEmpty() ? 0.0f : 0.85f});
    });

    process->start(m_settings->whisperExecutable(), {QStringLiteral("-f"), waveFile});
}

QString WhisperSttEngine::writeWaveFile(const QByteArray &pcmData) const
{
    const auto tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempRoot);
    const auto path = tempRoot + QStringLiteral("/jarvis_input_%1.wav").arg(QDateTime::currentMSecsSinceEpoch());

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        writeWaveHeader(file, static_cast<quint32>(pcmData.size()));
        file.write(pcmData);
    }

    return path;
}
