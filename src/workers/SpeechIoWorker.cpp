#include "workers/SpeechIoWorker.h"

#include "logging/LoggingService.h"
#include "settings/AppSettings.h"
#include "stt/WhisperSttEngine.h"
#include "tts/TtsEngineFactory.h"

SpeechIoWorker::SpeechIoWorker(AppSettings *settings, LoggingService *loggingService, QObject *parent)
    : QObject(parent)
{
    m_whisper = new WhisperSttEngine(settings, loggingService, this);
    m_tts = TtsEngineFactory::create(settings, loggingService, this);

    connect(m_whisper, &WhisperSttEngine::transcriptionReady, this, [this](quint64, const TranscriptionResult &result) {
        emit transcriptionReady(m_sttGeneration, result);
    });
    connect(m_whisper, &WhisperSttEngine::transcriptionFailed, this, [this](quint64, const QString &errorText) {
        emit transcriptionFailed(m_sttGeneration, errorText);
    });
    connect(m_tts, &TtsEngine::playbackStarted, this, [this]() {
        emit playbackStarted(m_activePlaybackGeneration);
    });
    connect(m_tts, &TtsEngine::playbackFinished, this, [this]() {
        emit playbackFinished(m_activePlaybackGeneration);
    });
    connect(m_tts, &TtsEngine::playbackFailed, this, [this](const QString &errorText) {
        emit playbackFailed(m_activePlaybackGeneration, errorText);
    });
    connect(m_tts, &TtsEngine::farEndFrameReady, this, [this](const AudioFrame &frame) {
        emit farEndFrameReady(m_activePlaybackGeneration, frame);
    });
}

void SpeechIoWorker::transcribe(quint64 generationId, const QByteArray &pcmData, const QString &initialPrompt, bool suppressNonSpeechTokens)
{
    m_sttGeneration = generationId;
    m_whisper->transcribePcm(pcmData, initialPrompt, suppressNonSpeechTokens);
}

void SpeechIoWorker::speak(quint64 generationId, const QString &text, const TtsUtteranceContext &context)
{
    m_activePlaybackGeneration = generationId;
    m_tts->speakText(text, context);
}

void SpeechIoWorker::cancel()
{
    ++m_activePlaybackGeneration;
    ++m_sttGeneration;
    m_tts->clear();
}
