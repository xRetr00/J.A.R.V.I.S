#include "wakeword/WakeWordEnginePrecise.h"

#include <QFileInfo>
#include <QIODevice>
#include <QMediaDevices>

#include "logging/LoggingService.h"

namespace {
constexpr int kSampleRate = 16000;
}

WakeWordEnginePrecise::WakeWordEnginePrecise(LoggingService *loggingService, QObject *parent)
    : QObject(parent)
    , m_loggingService(loggingService)
{
    connect(&m_engineProcess, &QProcess::readyReadStandardOutput, this, &WakeWordEnginePrecise::consumeEngineOutput);
    connect(&m_engineProcess, &QProcess::readyReadStandardError, this, [this]() {
        const QString stderrText = QString::fromUtf8(m_engineProcess.readAllStandardError()).trimmed();
        if (!stderrText.isEmpty() && m_loggingService) {
            m_loggingService->warn(QStringLiteral("precise-engine stderr: %1").arg(stderrText));
        }
    });
    connect(&m_engineProcess, &QProcess::errorOccurred, this, &WakeWordEnginePrecise::handleProcessError);
}

WakeWordEnginePrecise::~WakeWordEnginePrecise()
{
    stop();
}

bool WakeWordEnginePrecise::start(
    const QString &enginePath,
    const QString &modelPath,
    float threshold,
    int cooldownMs,
    const QString &preferredDeviceId)
{
    stop();

    if (!QFileInfo::exists(enginePath)) {
        emit errorOccurred(QStringLiteral("Mycroft Precise engine is missing"));
        return false;
    }
    if (!QFileInfo::exists(modelPath)) {
        emit errorOccurred(QStringLiteral("Wake word model is not trained yet"));
        return false;
    }
    if (!QFileInfo::exists(modelPath + QStringLiteral(".params"))) {
        emit errorOccurred(QStringLiteral("Wake word model params are missing"));
        return false;
    }

    m_threshold = threshold;
    m_cooldownMs = cooldownMs;
    m_lastActivationMs = 0;
    m_pendingAudio.clear();
    m_stdoutBuffer.clear();

    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!preferredDeviceId.isEmpty()) {
        for (const QAudioDevice &candidate : QMediaDevices::audioInputs()) {
            if (QString::fromUtf8(candidate.id()) == preferredDeviceId) {
                device = candidate;
                break;
            }
        }
    }

    if (device.isNull()) {
        emit errorOccurred(QStringLiteral("No microphone available for Mycroft Precise"));
        return false;
    }

    if (!device.isFormatSupported(m_format)) {
        emit errorOccurred(QStringLiteral("Microphone does not support 16 kHz mono PCM for wake detection"));
        return false;
    }

    m_engineProcess.start(enginePath, {QFileInfo(modelPath).absoluteFilePath(), QString::number(m_chunkBytes)});
    if (!m_engineProcess.waitForStarted(5000)) {
        emit errorOccurred(QStringLiteral("Failed to start Mycroft Precise engine"));
        return false;
    }

    m_audioSource = std::make_unique<QAudioSource>(device, m_format, this);
    m_audioSource->setBufferSize(m_chunkBytes * 4);
    m_audioIoDevice = m_audioSource->start();
    if (!m_audioIoDevice) {
        stop();
        emit errorOccurred(QStringLiteral("Failed to start microphone capture for wake detection"));
        return false;
    }

    m_audioReadyReadConnection = connect(m_audioIoDevice, &QIODevice::readyRead, this, &WakeWordEnginePrecise::flushAudioToEngine);
    if (m_loggingService) {
        m_loggingService->info(QStringLiteral("Mycroft Precise wake engine started. engine=\"%1\" model=\"%2\" threshold=%3 cooldownMs=%4")
            .arg(enginePath, modelPath)
            .arg(m_threshold, 0, 'f', 2)
            .arg(m_cooldownMs));
    }
    return true;
}

void WakeWordEnginePrecise::stop()
{
    if (m_audioReadyReadConnection) {
        disconnect(m_audioReadyReadConnection);
        m_audioReadyReadConnection = {};
    }

    if (m_audioSource) {
        m_audioSource->stop();
        m_audioSource.reset();
    }
    m_audioIoDevice = nullptr;
    m_pendingAudio.clear();
    m_stdoutBuffer.clear();

    if (m_engineProcess.state() != QProcess::NotRunning) {
        m_engineProcess.closeWriteChannel();
        m_engineProcess.terminate();
        if (!m_engineProcess.waitForFinished(1000)) {
            m_engineProcess.kill();
            m_engineProcess.waitForFinished(1000);
        }
    }
}

bool WakeWordEnginePrecise::isActive() const
{
    return m_audioSource != nullptr && m_engineProcess.state() == QProcess::Running;
}

void WakeWordEnginePrecise::flushAudioToEngine()
{
    if (!m_audioIoDevice || m_engineProcess.state() != QProcess::Running) {
        return;
    }

    m_pendingAudio.append(m_audioIoDevice->readAll());
    while (m_pendingAudio.size() >= m_chunkBytes) {
        const QByteArray chunk = m_pendingAudio.left(m_chunkBytes);
        m_pendingAudio.remove(0, m_chunkBytes);
        if (m_engineProcess.write(chunk) != chunk.size()) {
            emit errorOccurred(QStringLiteral("Failed to stream audio into Mycroft Precise engine"));
            stop();
            return;
        }
    }
}

void WakeWordEnginePrecise::consumeEngineOutput()
{
    m_stdoutBuffer.append(m_engineProcess.readAllStandardOutput());
    while (true) {
        const int newlineIndex = m_stdoutBuffer.indexOf('\n');
        if (newlineIndex < 0) {
            break;
        }

        const QByteArray rawLine = m_stdoutBuffer.left(newlineIndex).trimmed();
        m_stdoutBuffer.remove(0, newlineIndex + 1);
        if (rawLine.isEmpty()) {
            continue;
        }

        bool ok = false;
        const float probability = rawLine.toFloat(&ok);
        if (!ok) {
            if (m_loggingService) {
                m_loggingService->warn(QStringLiteral("Unexpected precise-engine output: %1").arg(QString::fromUtf8(rawLine)));
            }
            continue;
        }

        emit probabilityUpdated(probability);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (probability >= m_threshold && (nowMs - m_lastActivationMs) >= m_cooldownMs) {
            m_lastActivationMs = nowMs;
            if (m_loggingService) {
                m_loggingService->info(QStringLiteral("Mycroft Precise wake word detected. probability=%1")
                    .arg(probability, 0, 'f', 3));
            }
            emit wakeWordDetected();
        }
    }
}

void WakeWordEnginePrecise::handleProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    const QString message = m_engineProcess.errorString().isEmpty()
        ? QStringLiteral("Mycroft Precise engine failed")
        : QStringLiteral("Mycroft Precise engine failed: %1").arg(m_engineProcess.errorString());
    if (m_loggingService) {
        m_loggingService->error(message);
    }
    emit errorOccurred(message);
    stop();
}
