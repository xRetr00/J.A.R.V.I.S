#pragma once

#include <QBuffer>
#include <QAudioFormat>
#include <QFutureWatcher>
#include <QMetaObject>
#include <memory>
#include <QPointer>
#include <QQueue>
#include <QString>

#include "tts/TtsEngine.h"

struct TtsSynthesisResult
{
    QString outputFile;
    QString errorText;
    quint64 generation = 0;
    qint64 synthesisMs = 0;
    qint64 ffmpegMs = 0;
    qint64 totalMs = 0;
    QString postProcessMode;
};

class AppSettings;
class LoggingService;
class QAudioSink;
class QTimer;
class SpeechPreparationPipeline;

class PiperTtsEngine : public TtsEngine
{
    Q_OBJECT

public:
    explicit PiperTtsEngine(AppSettings *settings,
                            LoggingService *loggingService,
                            QObject *parent = nullptr);
    ~PiperTtsEngine() override;

    void speakText(const QString &text, const TtsUtteranceContext &context = {}) override;
    void clear() override;
    bool isSpeaking() const override;

private:
    void applySelectedOutputDevice();
    void processNext();
    TtsSynthesisResult synthesizeAndProcess(const QString &text, quint64 generation) const;
    void playFile(const QString &path);
    void stopPlayback();

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
    QQueue<QString> m_pendingTexts;
    bool m_processing = false;
    QFutureWatcher<TtsSynthesisResult> m_synthesisWatcher;
    QAudioSink *m_audioSink = nullptr;
    QBuffer *m_playbackBuffer = nullptr;
    QByteArray m_playbackPcm;
    QAudioFormat m_playbackFormat;
    QTimer *m_farEndTimer = nullptr;
    QMetaObject::Connection m_audioSinkStateConnection;
    qint64 m_lastFarEndOffset = 0;
    quint64 m_generationCounter = 0;
    quint64 m_activeGeneration = 0;
    qint64 m_activeRequestStartMs = 0;
    bool m_playbackStartedEmitted = false;
    std::unique_ptr<SpeechPreparationPipeline> m_speechPreparationPipeline;
};
