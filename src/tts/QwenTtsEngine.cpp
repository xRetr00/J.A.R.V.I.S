#include "tts/QwenTtsEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QtConcurrent>
#include <QAudio>
#include <QAudioDevice>
#include <QAudioSink>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include "logging/LoggingService.h"
#include "settings/AppSettings.h"
#include "tts/SpeechPreparationPipeline.h"

namespace {
QString clipForLog(QString text, int maxChars = 1200)
{
    if (text.size() > maxChars) {
        text = text.left(maxChars) + QStringLiteral(" ...[truncated]");
    }
    return text;
}

bool parseWaveFile(const QString &path, QByteArray *pcmData, QAudioFormat *format)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.size() < 44 || !data.startsWith("RIFF") || data.mid(8, 4) != "WAVE") {
        return false;
    }

    int offset = 12;
    quint16 channels = 1;
    quint32 sampleRate = 22050;
    quint16 bitsPerSample = 16;
    int dataOffset = -1;
    int dataSize = 0;

    while (offset + 8 <= data.size()) {
        const QByteArray chunkId = data.mid(offset, 4);
        quint32 chunkSize = 0;
        std::memcpy(&chunkSize, data.constData() + offset + 4, sizeof(chunkSize));
        offset += 8;

        if (chunkId == "fmt " && offset + 16 <= data.size()) {
            std::memcpy(&channels, data.constData() + offset + 2, sizeof(channels));
            std::memcpy(&sampleRate, data.constData() + offset + 4, sizeof(sampleRate));
            std::memcpy(&bitsPerSample, data.constData() + offset + 14, sizeof(bitsPerSample));
        } else if (chunkId == "data") {
            dataOffset = offset;
            dataSize = static_cast<int>(chunkSize);
            break;
        }

        offset += static_cast<int>(chunkSize);
    }

    if (dataOffset < 0 || dataSize <= 0 || dataOffset + dataSize > data.size()) {
        return false;
    }

    format->setSampleRate(static_cast<int>(sampleRate));
    format->setChannelCount(static_cast<int>(channels));
    format->setSampleFormat(bitsPerSample == 16 ? QAudioFormat::Int16 : QAudioFormat::UInt8);
    *pcmData = data.mid(dataOffset, dataSize);
    return true;
}

bool hasQwenModelFile(const QString &modelDir, const QString &pattern)
{
    const QDir dir(modelDir);
    if (!dir.exists()) {
        return false;
    }
    return !dir.entryList({pattern}, QDir::Files).isEmpty();
}
}

QwenTtsEngine::QwenTtsEngine(AppSettings *settings,
                             LoggingService *loggingService,
                             QObject *parent)
    : TtsEngine(parent)
    , m_settings(settings)
    , m_loggingService(loggingService)
{
    m_farEndTimer = new QTimer(this);
    m_farEndTimer->setInterval(10);
    connect(m_farEndTimer, &QTimer::timeout, this, [this]() {
        if (m_playbackBuffer == nullptr || m_playbackPcm.isEmpty()) {
            return;
        }

        const qint64 currentOffset = std::clamp(m_playbackBuffer->pos(), static_cast<qint64>(0), static_cast<qint64>(m_playbackPcm.size()));
        if (m_playbackFormat.sampleRate() <= 0 || m_playbackFormat.bytesPerSample() <= 0) {
            return;
        }

        const int chunkSamples = std::clamp((m_playbackFormat.sampleRate() * 30) / 1000, 1, AudioFrame::kMaxSamples);
        const int channelCount = std::max(1, m_playbackFormat.channelCount());
        const qint64 bytesPerFrame = static_cast<qint64>(chunkSamples)
            * static_cast<qint64>(m_playbackFormat.bytesPerSample())
            * static_cast<qint64>(channelCount);
        while (m_lastFarEndOffset + bytesPerFrame <= currentOffset) {
            AudioFrame frame;
            frame.sampleRate = m_playbackFormat.sampleRate();
            frame.channels = 1;
            frame.sampleCount = chunkSamples;
            const auto *samples = reinterpret_cast<const qint16 *>(m_playbackPcm.constData() + m_lastFarEndOffset);
            for (int i = 0; i < frame.sampleCount; ++i) {
                int mixed = 0;
                for (int channel = 0; channel < channelCount; ++channel) {
                    mixed += samples[(i * channelCount) + channel];
                }
                frame.samples[static_cast<std::size_t>(i)] = static_cast<float>(mixed) / static_cast<float>(channelCount * 32768.0f);
            }
            emit farEndFrameReady(frame);
            m_lastFarEndOffset += bytesPerFrame;
        }
    });

    connect(&m_synthesisWatcher, &QFutureWatcher<QwenTtsSynthesisResult>::finished, this, [this]() {
        const QwenTtsSynthesisResult result = m_synthesisWatcher.result();
        if (result.generation != m_activeGeneration || !m_processing) {
            if (m_loggingService) {
                m_loggingService->infoFor(QStringLiteral("tts"),
                                          QStringLiteral("[qwen_tts_drop] reason=stale_synthesis_result generation=%1 active=%2")
                                              .arg(QString::number(result.generation), QString::number(m_activeGeneration)));
            }
            return;
        }

        if (result.outputFile.isEmpty()) {
            m_processing = false;
            m_activeGeneration = 0;
            if (m_loggingService) {
                m_loggingService->warnFor(QStringLiteral("tts"),
                                          QStringLiteral("[qwen_tts_synthesis_failed] generation=%1 detail=%2")
                                              .arg(QString::number(result.generation), clipForLog(result.errorText, 400)));
            }
            emit playbackFailed(result.errorText.trimmed().isEmpty()
                ? QStringLiteral("Qwen TTS synthesis failed")
                : result.errorText.trimmed());
            return;
        }

        playFile(result.outputFile);
    });

    const int dedupeWindowMs = m_settings != nullptr ? m_settings->ttsDedupeWindowMs() : 7000;
    m_speechPreparationPipeline = std::make_unique<SpeechPreparationPipeline>(dedupeWindowMs);
}

QwenTtsEngine::~QwenTtsEngine() = default;

void QwenTtsEngine::speakText(const QString &text, const TtsUtteranceContext &context)
{
    if (m_speechPreparationPipeline == nullptr) {
        m_speechPreparationPipeline = std::make_unique<SpeechPreparationPipeline>(7000);
    }
    if (m_settings != nullptr) {
        m_speechPreparationPipeline->setDedupeWindowMs(m_settings->ttsDedupeWindowMs());
    }

    const SpeechPreparationTrace trace = m_speechPreparationPipeline->prepare(text, context);
    const QString prepared = trace.finalSpokenText;
    if (m_loggingService) {
        m_loggingService->infoFor(
            QStringLiteral("tts"),
            QStringLiteral("[qwen_tts_pipeline] rawChars=%1 finalChars=%2 class=%3 source=%4 turn=%5 target=%6 admitted=%7 reason=%8")
                .arg(QString::number(trace.rawInputText.size()),
                     QString::number(prepared.size()),
                     context.utteranceClass,
                     context.source,
                     context.turnId,
                     context.semanticTarget,
                     trace.dedupeDecision.admitted ? QStringLiteral("true") : QStringLiteral("false"),
                     trace.dedupeDecision.reason));
    }

    if (trace.emptyAfterPreparation || trace.statusOnly || !trace.dedupeDecision.admitted) {
        emit playbackFinished();
        return;
    }

    m_pendingTexts.enqueue(prepared);
    if (!m_processing) {
        processNext();
    }
}

void QwenTtsEngine::clear()
{
    ++m_generationCounter;
    m_activeGeneration = 0;
    m_pendingTexts.clear();
    m_processing = false;
    stopPlayback();
    if (m_loggingService) {
        m_loggingService->infoFor(QStringLiteral("tts"), QStringLiteral("[qwen_tts_clear] queue_cleared=true"));
    }
}

bool QwenTtsEngine::isSpeaking() const
{
    return m_processing || !m_pendingTexts.isEmpty();
}

void QwenTtsEngine::processNext()
{
    if (m_pendingTexts.isEmpty()) {
        m_processing = false;
        m_activeGeneration = 0;
        emit playbackFinished();
        return;
    }

    m_processing = true;
    const QString sentence = m_pendingTexts.dequeue();
    const quint64 generation = ++m_generationCounter;
    m_activeGeneration = generation;
    if (m_loggingService) {
        m_loggingService->infoFor(QStringLiteral("tts"),
                                  QStringLiteral("[qwen_tts_synth_start] generation=%1 queueRemaining=%2 text=%3")
                                      .arg(QString::number(generation),
                                           QString::number(m_pendingTexts.size()),
                                           clipForLog(sentence)));
    }
    m_synthesisWatcher.setFuture(QtConcurrent::run([this, sentence, generation]() {
        return synthesize(sentence, generation);
    }));
}

QwenTtsSynthesisResult QwenTtsEngine::synthesize(const QString &sentence, quint64 generation) const
{
    QwenTtsSynthesisResult result;
    result.generation = generation;

    const QString executable = m_settings != nullptr ? m_settings->qwenTtsExecutable().trimmed() : QString{};
    const QString modelDir = m_settings != nullptr ? m_settings->qwenTtsModelDir().trimmed() : QString{};
    const QString language = m_settings != nullptr ? m_settings->qwenTtsLanguage().trimmed() : QStringLiteral("en");
    const int threads = m_settings != nullptr ? std::max(1, m_settings->qwenTtsThreads()) : 4;

    if (executable.isEmpty() || !QFileInfo::exists(executable)) {
        result.errorText = QStringLiteral("Qwen TTS executable is missing. Configure qwen3-tts-cli.");
        return result;
    }

    if (modelDir.isEmpty() || !QFileInfo(modelDir).isDir()) {
        result.errorText = QStringLiteral("Qwen TTS model directory is missing.");
        return result;
    }

    if (!hasQwenModelFile(modelDir, QStringLiteral("qwen3-tts-0.6b*.gguf"))
        || !hasQwenModelFile(modelDir, QStringLiteral("qwen3-tts-tokenizer*.gguf"))) {
        result.errorText = QStringLiteral("Qwen TTS model directory does not contain required GGUF files.");
        return result;
    }

    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempRoot);
    const QString token = QStringLiteral("%1_%2")
                              .arg(QString::number(generation), QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString outputPath = tempRoot + QStringLiteral("/vaxil_qwen_tts_%1.wav").arg(token);

    QProcess process;
    QStringList args{
        QStringLiteral("-m"), modelDir,
        QStringLiteral("-t"), sentence,
        QStringLiteral("-o"), outputPath,
        QStringLiteral("-l"), language.isEmpty() ? QStringLiteral("en") : language,
        QStringLiteral("--max-tokens"), QStringLiteral("2048"),
        QStringLiteral("-j"), QString::number(threads)
    };
    process.start(executable, args);
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(1000);
        result.errorText = QStringLiteral("Qwen TTS synthesis timed out.");
        return result;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        result.errorText = stderrText.isEmpty()
            ? QStringLiteral("Qwen TTS synthesis failed with exit code %1").arg(process.exitCode())
            : QStringLiteral("Qwen TTS synthesis failed: %1").arg(stderrText);
        return result;
    }

    if (!QFileInfo::exists(outputPath) || QFileInfo(outputPath).size() <= 44) {
        result.errorText = QStringLiteral("Qwen TTS synthesis returned empty audio.");
        return result;
    }

    result.outputFile = outputPath;
    return result;
}

void QwenTtsEngine::playFile(const QString &path)
{
    stopPlayback();

    QByteArray pcmData;
    QAudioFormat format;
    if (!parseWaveFile(path, &pcmData, &format)) {
        m_processing = false;
        if (m_loggingService) {
            m_loggingService->warnFor(QStringLiteral("tts"),
                                      QStringLiteral("[qwen_tts_playback_failed] reason=parse_wave_failed path=%1").arg(path));
        }
        emit playbackFailed(QStringLiteral("Qwen TTS produced invalid WAV output."));
        return;
    }

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    const QString selectedId = m_settings != nullptr ? m_settings->selectedAudioOutputDeviceId() : QString{};
    if (!selectedId.isEmpty()) {
        for (const QAudioDevice &candidate : QMediaDevices::audioOutputs()) {
            if (QString::fromUtf8(candidate.id()) == selectedId) {
                device = candidate;
                break;
            }
        }
    }

    m_audioSink = new QAudioSink(device, format, this);
    m_playbackPcm = pcmData;
    m_playbackFormat = format;
    m_playbackBuffer = new QBuffer(this);
    m_playbackBuffer->setData(m_playbackPcm);
    m_playbackBuffer->open(QIODevice::ReadOnly);
    m_lastFarEndOffset = 0;

    QPointer<QAudioSink> audioSink = m_audioSink;
    m_audioSinkStateConnection = connect(m_audioSink, &QAudioSink::stateChanged, this, [this, audioSink](QAudio::State state) {
        if (!audioSink || audioSink != m_audioSink) {
            return;
        }

        if (state == QAudio::IdleState) {
            stopPlayback();
            processNext();
        } else if (state == QAudio::StoppedState && m_audioSink != nullptr && m_audioSink->error() != QAudio::NoError) {
            stopPlayback();
            m_processing = false;
            emit playbackFailed(QStringLiteral("Qwen TTS audio playback failed."));
        }
    });

    if (m_loggingService) {
        m_loggingService->infoFor(QStringLiteral("tts"),
                                  QStringLiteral("[qwen_tts_playback_start] sampleRate=%1 channels=%2 bytes=%3")
                                      .arg(QString::number(format.sampleRate()),
                                           QString::number(format.channelCount()),
                                           QString::number(pcmData.size())));
    }
    emit playbackStarted();
    m_audioSink->start(m_playbackBuffer);
    m_farEndTimer->start();
}

void QwenTtsEngine::stopPlayback()
{
    if (m_farEndTimer) {
        m_farEndTimer->stop();
    }
    m_lastFarEndOffset = 0;
    m_playbackFormat = QAudioFormat();
    QAudioSink *audioSink = m_audioSink;
    QBuffer *playbackBuffer = m_playbackBuffer;
    m_audioSink = nullptr;
    m_playbackBuffer = nullptr;
    if (m_audioSinkStateConnection) {
        disconnect(m_audioSinkStateConnection);
        m_audioSinkStateConnection = {};
    }
    if (audioSink != nullptr) {
        audioSink->stop();
    }
    if (playbackBuffer != nullptr) {
        QObject *bufferOwner = audioSink != nullptr ? static_cast<QObject *>(audioSink) : static_cast<QObject *>(this);
        playbackBuffer->setParent(bufferOwner);
        QTimer::singleShot(0, playbackBuffer, [playbackBuffer]() {
            playbackBuffer->close();
            playbackBuffer->deleteLater();
        });
    }
    if (audioSink != nullptr) {
        audioSink->deleteLater();
    }
    m_playbackPcm.clear();
}
