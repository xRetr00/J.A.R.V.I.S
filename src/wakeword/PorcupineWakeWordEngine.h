#pragma once

#include <QAudioFormat>
#include <QAudioSource>
#include <QByteArray>
#include <QLibrary>
#include <QObject>
#include <QPointer>
#include <memory>

#include <picovoice.h>
#include <pv_porcupine.h>

class LoggingService;

class PorcupineWakeWordEngine : public QObject
{
    Q_OBJECT

public:
    explicit PorcupineWakeWordEngine(LoggingService *loggingService, QObject *parent = nullptr);
    ~PorcupineWakeWordEngine() override;

    bool start(
        const QString &accessKey,
        const QString &libraryPath,
        const QString &modelPath,
        const QString &keywordPath,
        float sensitivity,
        const QString &preferredDeviceId = {});
    void stop();
    bool isActive() const;

signals:
    void wakeWordDetected();
    void errorOccurred(const QString &message);

private:
    using PvSampleRateFn = int32_t (*)();
    using PvStatusToStringFn = const char *(*)(pv_status_t);
    using PvGetErrorStackFn = pv_status_t (*)(char ***, int32_t *);
    using PvFreeErrorStackFn = void (*)(char **);
    using PvPorcupineInitFn = pv_status_t (*)(const char *, const char *, const char *, int32_t, const char *const *, const float *, pv_porcupine_t **);
    using PvPorcupineDeleteFn = void (*)(pv_porcupine_t *);
    using PvPorcupineProcessFn = pv_status_t (*)(pv_porcupine_t *, const int16_t *, int32_t *);
    using PvPorcupineFrameLengthFn = int32_t (*)();

    bool loadRuntime(const QString &libraryPath);
    void unloadRuntime();
    QString describeStatus(const QString &operation, pv_status_t status) const;
    void processBuffer();

    LoggingService *m_loggingService = nullptr;
    std::unique_ptr<QAudioSource> m_audioSource;
    QPointer<QIODevice> m_ioDevice;
    QAudioFormat m_format;
    QByteArray m_pendingPcm;
    QMetaObject::Connection m_readyReadConnection;
    QLibrary m_runtimeLibrary;
    pv_porcupine_t *m_handle = nullptr;
    int32_t m_frameLength = 0;

    PvSampleRateFn m_pvSampleRate = nullptr;
    PvStatusToStringFn m_pvStatusToString = nullptr;
    PvGetErrorStackFn m_pvGetErrorStack = nullptr;
    PvFreeErrorStackFn m_pvFreeErrorStack = nullptr;
    PvPorcupineInitFn m_pvPorcupineInit = nullptr;
    PvPorcupineDeleteFn m_pvPorcupineDelete = nullptr;
    PvPorcupineProcessFn m_pvPorcupineProcess = nullptr;
    PvPorcupineFrameLengthFn m_pvPorcupineFrameLength = nullptr;
};
