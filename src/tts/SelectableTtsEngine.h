#pragma once

#include "tts/TtsEngine.h"

class AppSettings;
class LoggingService;
class PiperTtsEngine;
class QwenTtsEngine;

class SelectableTtsEngine : public TtsEngine
{
    Q_OBJECT

public:
    explicit SelectableTtsEngine(AppSettings *settings,
                                 LoggingService *loggingService,
                                 PiperTtsEngine *piperEngine,
                                 QwenTtsEngine *qwenEngine,
                                 QObject *parent = nullptr);

    void speakText(const QString &text, const TtsUtteranceContext &context = {}) override;
    void clear() override;
    bool isSpeaking() const override;

private:
    enum class ActiveBackend {
        None,
        Piper,
        Qwen
    };

    void startPiper(const QString &text, const TtsUtteranceContext &context, const QString &reason);
    bool shouldFallbackToPiper() const;

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
    PiperTtsEngine *m_piperEngine = nullptr;
    QwenTtsEngine *m_qwenEngine = nullptr;
    ActiveBackend m_activeBackend = ActiveBackend::None;
    QString m_pendingText;
    TtsUtteranceContext m_pendingContext;
    bool m_qwenPlaybackStarted = false;
    bool m_fallbackAttempted = false;
    bool m_cancelled = false;
};
