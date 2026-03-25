#pragma once

#include <QDateTime>
#include <memory>

#include "wakeword/WakeWordEngine.h"

class AppSettings;
class LoggingService;

#if JARVIS_HAS_SHERPA_ONNX
namespace sherpa_onnx::cxx {
class KeywordSpotter;
class OnlineStream;
}
#endif

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
    bool usesExternalAudioInput() const override { return true; }
    void processAudioFrame(const AudioFrame &frame) override;

private:
    bool prepareKeywordSpotter(const QString &runtimePath, const QString &modelPath, float threshold);
    bool writeKeywordsFile(const QString &modelPath, QString *keywordsPath, QString *errorText) const;
    void resetDetectorState();

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
    qint64 m_lastActivationMs = 0;
    float m_threshold = 0.25f;
    int m_cooldownMs = 900;
    QString m_preferredDeviceId;
    qint64 m_ignoreDetectionsUntilMs = 0;
    int m_activationWarmupMs = 1500;
    bool m_paused = false;

#if JARVIS_HAS_SHERPA_ONNX
    std::unique_ptr<sherpa_onnx::cxx::KeywordSpotter> m_keywordSpotter;
    std::unique_ptr<sherpa_onnx::cxx::OnlineStream> m_stream;
    QString m_runtimeRoot;
    QString m_modelRoot;
#endif
};
