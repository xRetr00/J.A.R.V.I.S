#pragma once

#include <QAudioFormat>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QQueue>
#include <QString>
#include <memory>

#include "tts/TtsEngine.h"

struct QwenTtsSynthesisResult
{
    QString outputFile;
    QString errorText;
    quint64 generation = 0;
};

class AppSettings;
class LoggingService;
class QAudioSink;
class QBuffer;
class QTimer;
class SpeechPreparationPipeline;

class QwenTtsEngine : public TtsEngine
{
    Q_OBJECT

public:
    explicit QwenTtsEngine(AppSettings *settings,
                           LoggingService *loggingService,
                           QObject *parent = nullptr);
    ~QwenTtsEngine() override;

    void speakText(const QString &text, const TtsUtteranceContext &context = {}) override;
    void clear() override;
    bool isSpeaking() const override;

private:
    void processNext();
    QwenTtsSynthesisResult synthesize(const QString &text, quint64 generation) const;
    void playFile(const QString &path);
    void stopPlayback();

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
    QQueue<QString> m_pendingTexts;
    bool m_processing = false;
    QFutureWatcher<QwenTtsSynthesisResult> m_synthesisWatcher;
    QAudioSink *m_audioSink = nullptr;
    QBuffer *m_playbackBuffer = nullptr;
    QByteArray m_playbackPcm;
    QAudioFormat m_playbackFormat;
    QTimer *m_farEndTimer = nullptr;
    QMetaObject::Connection m_audioSinkStateConnection;
    qint64 m_lastFarEndOffset = 0;
    quint64 m_generationCounter = 0;
    quint64 m_activeGeneration = 0;
    std::unique_ptr<SpeechPreparationPipeline> m_speechPreparationPipeline;
};
