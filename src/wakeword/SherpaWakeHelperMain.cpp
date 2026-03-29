#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QIODevice>
#include <QMediaDevices>
#include <QScopedPointer>
#include <QThread>
#include <QTextStream>

#if JARVIS_HAS_SHERPA_ONNX
#include <sherpa-onnx/c-api/cxx-api.h>
#endif

namespace {
constexpr int kSampleRate = 16000;
constexpr int kFrameMs = 30;
constexpr int kFrameSamples = 480;
constexpr int kFrameBytes = kFrameSamples * static_cast<int>(sizeof(qint16));

class SherpaWakeHelper final : public QObject
{
public:
    struct Config {
        QString encoderPath;
        QString decoderPath;
        QString joinerPath;
        QString tokensPath;
        QString keywordsFilePath;
        QString deviceId;
        float threshold = 0.18f;
        int cooldownMs = 450;
        int warmupMs = 250;
    };

    explicit SherpaWakeHelper(const Config &config, QObject *parent = nullptr)
        : QObject(parent)
        , m_config(config)
    {
        m_format.setSampleRate(kSampleRate);
        m_format.setChannelCount(1);
        m_format.setSampleFormat(QAudioFormat::Int16);
    }

    int start()
    {
#if !JARVIS_HAS_SHERPA_ONNX
        QTextStream(stderr) << "ERROR: sherpa-onnx support is not compiled into this build" << Qt::endl;
        return 1;
#else
        sherpa_onnx::cxx::KeywordSpotterConfig config;
        config.feat_config.sample_rate = kSampleRate;
        config.feat_config.feature_dim = 80;
        config.model_config.transducer.encoder = m_config.encoderPath.toStdString();
        config.model_config.transducer.decoder = m_config.decoderPath.toStdString();
        config.model_config.transducer.joiner = m_config.joinerPath.toStdString();
        config.model_config.tokens = m_config.tokensPath.toStdString();
        config.model_config.provider = "cpu";
        int numThreads = QThread::idealThreadCount();
        if (numThreads < 1) {
            numThreads = 1;
        } else if (numThreads > 4) {
            numThreads = 4;
        }
        config.model_config.num_threads = numThreads;
        config.model_config.model_type = "";
        config.keywords_file = m_config.keywordsFilePath.toStdString();
        config.keywords_threshold = m_config.threshold;
        config.keywords_score = 1.0f;
        config.max_active_paths = 8;
        config.num_trailing_blanks = 1;

        try {
            m_keywordSpotter.reset(new sherpa_onnx::cxx::KeywordSpotter(
                sherpa_onnx::cxx::KeywordSpotter::Create(config)));
            if (!m_keywordSpotter || m_keywordSpotter->Get() == nullptr) {
                QTextStream(stderr) << "ERROR: Failed to initialize sherpa keyword spotter" << Qt::endl;
                return 1;
            }

            m_stream.reset(new sherpa_onnx::cxx::OnlineStream(m_keywordSpotter->CreateStream()));
            if (!m_stream || m_stream->Get() == nullptr) {
                QTextStream(stderr) << "ERROR: Failed to create sherpa keyword stream" << Qt::endl;
                return 1;
            }
        } catch (...) {
            QTextStream(stderr) << "ERROR: Failed to initialize sherpa keyword spotter" << Qt::endl;
            return 1;
        }

        QAudioDevice device = QMediaDevices::defaultAudioInput();
        if (!m_config.deviceId.isEmpty()) {
            for (const QAudioDevice &candidate : QMediaDevices::audioInputs()) {
                if (QString::fromUtf8(candidate.id()) == m_config.deviceId) {
                    device = candidate;
                    break;
                }
            }
        }

        if (device.isNull()) {
            QTextStream(stderr) << "ERROR: No microphone available for wake detection" << Qt::endl;
            return 1;
        }
        if (!device.isFormatSupported(m_format)) {
            QTextStream(stderr) << "ERROR: Selected microphone does not support 16 kHz mono PCM for wake detection" << Qt::endl;
            return 1;
        }

        m_audioSource.reset(new QAudioSource(device, m_format, this));
        m_audioSource->setBufferSize(kFrameBytes * 2);
        m_audioIoDevice = m_audioSource->start();
        if (!m_audioIoDevice) {
            QTextStream(stderr) << "ERROR: Failed to start microphone capture for wake detection" << Qt::endl;
            return 1;
        }

        m_ignoreDetectionsUntilMs = QDateTime::currentMSecsSinceEpoch() + m_config.warmupMs;
        connect(m_audioIoDevice, &QIODevice::readyRead, this, [this]() { processMicBuffer(); }, Qt::DirectConnection);
        QTextStream(stdout) << "MIC:" << device.description() << Qt::endl;
        QTextStream(stdout) << "READY" << Qt::endl;
        return 0;
#endif
    }

private:
    void processMicBuffer()
    {
#if JARVIS_HAS_SHERPA_ONNX
        if (!m_audioIoDevice || !m_keywordSpotter || !m_stream) {
            return;
        }

        m_pendingPcm.append(m_audioIoDevice->readAll());
        while (m_pendingPcm.size() >= kFrameBytes) {
            const auto *samples = reinterpret_cast<const qint16 *>(m_pendingPcm.constData());
            float floatSamples[kFrameSamples];
            for (int i = 0; i < kFrameSamples; ++i) {
                floatSamples[i] = static_cast<float>(samples[i]) / 32768.0f;
            }
            m_pendingPcm.remove(0, kFrameBytes);

            sherpa_onnx::cxx::KeywordResult result;
            try {
                m_stream->AcceptWaveform(kSampleRate, floatSamples, kFrameSamples);
                while (m_keywordSpotter->IsReady(m_stream.get())) {
                    m_keywordSpotter->Decode(m_stream.get());
                }
                result = m_keywordSpotter->GetResult(m_stream.get());
            } catch (...) {
                QTextStream(stderr) << "ERROR: sherpa wake processing failed" << Qt::endl;
                QCoreApplication::exit(1);
                return;
            }

            if (result.keyword.empty()) {
                continue;
            }

            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (nowMs < m_ignoreDetectionsUntilMs || (nowMs - m_lastActivationMs) < m_config.cooldownMs) {
                m_keywordSpotter->Reset(m_stream.get());
                continue;
            }

            m_lastActivationMs = nowMs;
            QTextStream(stdout) << "DETECTED:" << QString::fromStdString(result.keyword) << Qt::endl;
            m_keywordSpotter->Reset(m_stream.get());
        }
#endif
    }

    Config m_config;
    QAudioFormat m_format;
    QScopedPointer<QAudioSource> m_audioSource;
    QIODevice *m_audioIoDevice = nullptr;
    QByteArray m_pendingPcm;
    qint64 m_lastActivationMs = 0;
    qint64 m_ignoreDetectionsUntilMs = 0;

#if JARVIS_HAS_SHERPA_ONNX
    QScopedPointer<sherpa_onnx::cxx::KeywordSpotter> m_keywordSpotter;
    QScopedPointer<sherpa_onnx::cxx::OnlineStream> m_stream;
#endif
};
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Vaxil sherpa wake helper"));
    parser.addHelpOption();

    QCommandLineOption encoder(QStringLiteral("encoder"), QStringLiteral("Path to encoder model"), QStringLiteral("path"));
    QCommandLineOption decoder(QStringLiteral("decoder"), QStringLiteral("Path to decoder model"), QStringLiteral("path"));
    QCommandLineOption joiner(QStringLiteral("joiner"), QStringLiteral("Path to joiner model"), QStringLiteral("path"));
    QCommandLineOption tokens(QStringLiteral("tokens"), QStringLiteral("Path to tokens.txt"), QStringLiteral("path"));
    QCommandLineOption keywords(QStringLiteral("keywords-file"), QStringLiteral("Path to sherpa keywords file"), QStringLiteral("path"));
    QCommandLineOption deviceId(QStringLiteral("device-id"), QStringLiteral("Preferred microphone device id"), QStringLiteral("id"));
    QCommandLineOption threshold(QStringLiteral("threshold"), QStringLiteral("Wake threshold"), QStringLiteral("value"), QStringLiteral("0.18"));
    QCommandLineOption cooldown(QStringLiteral("cooldown-ms"), QStringLiteral("Wake cooldown in milliseconds"), QStringLiteral("value"), QStringLiteral("450"));
    QCommandLineOption warmup(QStringLiteral("warmup-ms"), QStringLiteral("Wake warmup in milliseconds"), QStringLiteral("value"), QStringLiteral("250"));

    parser.addOption(encoder);
    parser.addOption(decoder);
    parser.addOption(joiner);
    parser.addOption(tokens);
    parser.addOption(keywords);
    parser.addOption(deviceId);
    parser.addOption(threshold);
    parser.addOption(cooldown);
    parser.addOption(warmup);
    parser.process(app);

    const QStringList requiredValues = {
        parser.value(encoder),
        parser.value(decoder),
        parser.value(joiner),
        parser.value(tokens),
        parser.value(keywords)
    };
    for (const QString &value : requiredValues) {
        if (value.trimmed().isEmpty()) {
            QTextStream(stderr) << "ERROR: Missing required sherpa wake helper arguments" << Qt::endl;
            return 1;
        }
    }

    SherpaWakeHelper helper({
        .encoderPath = parser.value(encoder),
        .decoderPath = parser.value(decoder),
        .joinerPath = parser.value(joiner),
        .tokensPath = parser.value(tokens),
        .keywordsFilePath = parser.value(keywords),
        .deviceId = parser.value(deviceId),
        .threshold = qBound(0.10f, parser.value(threshold).toFloat(), 0.85f),
        .cooldownMs = qMax(250, parser.value(cooldown).toInt()),
        .warmupMs = qMax(250, parser.value(warmup).toInt())
    });
    const int startCode = helper.start();
    if (startCode != 0) {
        return startCode;
    }

    return app.exec();
}
