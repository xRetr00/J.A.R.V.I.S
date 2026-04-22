#include "tts/SelectableTtsEngine.h"

#include "logging/LoggingService.h"
#include "settings/AppSettings.h"
#include "tts/PiperTtsEngine.h"
#include "tts/QwenTtsEngine.h"

SelectableTtsEngine::SelectableTtsEngine(AppSettings *settings,
                                         LoggingService *loggingService,
                                         PiperTtsEngine *piperEngine,
                                         QwenTtsEngine *qwenEngine,
                                         QObject *parent)
    : TtsEngine(parent)
    , m_settings(settings)
    , m_loggingService(loggingService)
    , m_piperEngine(piperEngine)
    , m_qwenEngine(qwenEngine)
{
    if (m_piperEngine != nullptr) {
        m_piperEngine->setParent(this);
    }
    if (m_qwenEngine != nullptr) {
        m_qwenEngine->setParent(this);
    }

    connect(m_piperEngine, &PiperTtsEngine::playbackStarted, this, [this]() {
        if (m_activeBackend == ActiveBackend::Piper) {
            emit playbackStarted();
        }
    });
    connect(m_piperEngine, &PiperTtsEngine::playbackFinished, this, [this]() {
        if (m_activeBackend == ActiveBackend::Piper) {
            m_activeBackend = ActiveBackend::None;
            emit playbackFinished();
        }
    });
    connect(m_piperEngine, &PiperTtsEngine::playbackFailed, this, [this](const QString &errorText) {
        if (m_activeBackend == ActiveBackend::Piper) {
            m_activeBackend = ActiveBackend::None;
            emit playbackFailed(errorText);
        }
    });
    connect(m_piperEngine, &PiperTtsEngine::farEndFrameReady, this, [this](const AudioFrame &frame) {
        if (m_activeBackend == ActiveBackend::Piper) {
            emit farEndFrameReady(frame);
        }
    });

    connect(m_qwenEngine, &QwenTtsEngine::playbackStarted, this, [this]() {
        if (m_activeBackend == ActiveBackend::Qwen) {
            m_qwenPlaybackStarted = true;
            emit playbackStarted();
        }
    });
    connect(m_qwenEngine, &QwenTtsEngine::playbackFinished, this, [this]() {
        if (m_activeBackend == ActiveBackend::Qwen) {
            m_activeBackend = ActiveBackend::None;
            emit playbackFinished();
        }
    });
    connect(m_qwenEngine, &QwenTtsEngine::playbackFailed, this, [this](const QString &errorText) {
        if (m_activeBackend != ActiveBackend::Qwen) {
            return;
        }

        if (shouldFallbackToPiper()) {
            startPiper(m_pendingText, m_pendingContext, errorText);
            return;
        }

        m_activeBackend = ActiveBackend::None;
        emit playbackFailed(errorText);
    });
    connect(m_qwenEngine, &QwenTtsEngine::farEndFrameReady, this, [this](const AudioFrame &frame) {
        if (m_activeBackend == ActiveBackend::Qwen) {
            emit farEndFrameReady(frame);
        }
    });
}

void SelectableTtsEngine::speakText(const QString &text, const TtsUtteranceContext &context)
{
    if (text.trimmed().isEmpty()) {
        return;
    }

    m_pendingText = text;
    m_pendingContext = context;
    m_qwenPlaybackStarted = false;
    m_fallbackAttempted = false;
    m_cancelled = false;

    const QString kind = m_settings != nullptr ? m_settings->ttsEngineKind().trimmed().toLower() : QStringLiteral("piper");
    if (kind == QStringLiteral("qwen")) {
        if (m_qwenEngine == nullptr) {
            startPiper(text, context, QStringLiteral("Qwen backend is unavailable."));
            return;
        }
        if (m_loggingService) {
            m_loggingService->infoFor(QStringLiteral("tts"), QStringLiteral("[tts_backend] selected=qwen"));
        }
        m_activeBackend = ActiveBackend::Qwen;
        m_qwenEngine->speakText(text, context);
        return;
    }

    if (m_piperEngine == nullptr) {
        m_activeBackend = ActiveBackend::None;
        emit playbackFailed(QStringLiteral("Piper backend is unavailable."));
        return;
    }

    if (m_loggingService) {
        m_loggingService->infoFor(QStringLiteral("tts"), QStringLiteral("[tts_backend] selected=piper"));
    }
    m_activeBackend = ActiveBackend::Piper;
    m_piperEngine->speakText(text, context);
}

void SelectableTtsEngine::clear()
{
    m_cancelled = true;
    m_activeBackend = ActiveBackend::None;
    m_qwenPlaybackStarted = false;
    m_fallbackAttempted = false;
    m_pendingText.clear();
    m_pendingContext = {};

    if (m_qwenEngine != nullptr) {
        m_qwenEngine->clear();
    }
    if (m_piperEngine != nullptr) {
        m_piperEngine->clear();
    }
}

bool SelectableTtsEngine::isSpeaking() const
{
    const bool qwenSpeaking = m_qwenEngine != nullptr && m_qwenEngine->isSpeaking();
    const bool piperSpeaking = m_piperEngine != nullptr && m_piperEngine->isSpeaking();
    return qwenSpeaking || piperSpeaking;
}

void SelectableTtsEngine::startPiper(const QString &text, const TtsUtteranceContext &context, const QString &reason)
{
    if (m_piperEngine == nullptr || m_cancelled) {
        m_activeBackend = ActiveBackend::None;
        emit playbackFailed(reason);
        return;
    }

    m_fallbackAttempted = true;
    m_activeBackend = ActiveBackend::Piper;
    if (m_loggingService) {
        m_loggingService->warnFor(QStringLiteral("tts"),
                                  QStringLiteral("[tts_backend_fallback] from=qwen to=piper reason=%1")
                                      .arg(reason.left(400)));
    }
    m_piperEngine->speakText(text, context);
}

bool SelectableTtsEngine::shouldFallbackToPiper() const
{
    // User requested strict safety: never replay through Piper after Qwen playback has started.
    return !m_cancelled && !m_fallbackAttempted && !m_qwenPlaybackStarted;
}
