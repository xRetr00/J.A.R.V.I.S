#pragma once

#include <QDateTime>
#include <QPointer>
#include <QProcess>

#include "wakeword/WakeWordEngine.h"

class AppSettings;
class LoggingService;

class SherpaWakeWordEngine : public WakeWordEngine
{
    Q_OBJECT

public:
    explicit SherpaWakeWordEngine(AppSettings *settings, LoggingService *loggingService, QObject *parent = nullptr);
    ~SherpaWakeWordEngine() override;

    bool start(
        const QString &enginePath,
        const QString &modelPath,
        float threshold,
        int cooldownMs,
        const QString &preferredDeviceId = {}) override;
    void pause() override;
    void resume() override;
    void stop() override;
    bool isActive() const override;
    bool isPaused() const override;
    bool usesExternalAudioInput() const override { return false; }

private:
    bool startHelperProcess();
    void consumeHelperStdout();
    void consumeHelperStderr();
    void handleHelperFinished(int exitCode, QProcess::ExitStatus exitStatus);
    QString resolveHelperExecutablePath() const;
    QString resolveModelFile(const QString &rootPath, const QStringList &fileNames) const;
    bool writeKeywordsFile(const QString &modelPath, QString *keywordsPath, QString *errorText) const;

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
    float m_threshold = 0.25f;
    int m_cooldownMs = 900;
    QString m_preferredDeviceId;
    bool m_paused = false;
    bool m_ready = false;
    bool m_stopRequested = false;
    int m_activationWarmupMs = 1500;
    QString m_runtimeRoot;
    QString m_modelRoot;
    QString m_keywordsFilePath;
    QString m_encoderPath;
    QString m_decoderPath;
    QString m_joinerPath;
    QString m_tokensPath;
    QString m_helperPath;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    QPointer<QProcess> m_helperProcess;
};
