#pragma once

#include <QAudioFormat>
#include <QAudioSource>
#include <QByteArray>
#include <QDateTime>
#include <QIODevice>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <memory>

class LoggingService;

class WakeWordEnginePrecise : public QObject
{
    Q_OBJECT

public:
    explicit WakeWordEnginePrecise(LoggingService *loggingService, QObject *parent = nullptr);
    ~WakeWordEnginePrecise() override;

    bool start(
        const QString &enginePath,
        const QString &modelPath,
        float threshold,
        int cooldownMs,
        const QString &preferredDeviceId = {});
    void stop();
    bool isActive() const;

signals:
    void probabilityUpdated(float probability);
    void wakeWordDetected();
    void errorOccurred(const QString &message);

private:
    void flushAudioToEngine();
    void consumeEngineOutput();
    void handleProcessError(QProcess::ProcessError error);

    LoggingService *m_loggingService = nullptr;
    std::unique_ptr<QAudioSource> m_audioSource;
    QPointer<QIODevice> m_audioIoDevice;
    QAudioFormat m_format;
    QByteArray m_pendingAudio;
    QByteArray m_stdoutBuffer;
    QMetaObject::Connection m_audioReadyReadConnection;
    QProcess m_engineProcess;
    qint64 m_lastActivationMs = 0;
    int m_chunkBytes = 2048;
    float m_threshold = 0.8f;
    int m_cooldownMs = 1500;
};
