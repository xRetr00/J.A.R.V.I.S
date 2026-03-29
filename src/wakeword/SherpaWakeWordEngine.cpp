#include "wakeword/SherpaWakeWordEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>

#include "logging/LoggingService.h"
#include "platform/PlatformRuntime.h"
#include "settings/AppSettings.h"

#if JARVIS_HAS_SHERPA_ONNX
#include <sentencepiece_processor.h>
#endif

namespace {
constexpr int kMinCooldownMs = 250;
constexpr int kMaxCooldownMs = 1600;
constexpr int kKeywordSegmentationCandidates = 6;

struct WakeKeywordVariant {
    QString phrase;
    float boostingScore = 1.0f;
    float triggerThreshold = 0.25f;
};

QString firstExisting(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    return {};
}

QString normalizeWakePhrase(const QString &phrase)
{
    QString normalized = phrase.trimmed().toUpper();
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    normalized.remove(QRegularExpression(QStringLiteral("[^A-Z0-9 ]")));
    return normalized.trimmed();
}

QString stripWakeGreeting(const QString &phrase)
{
    const QString normalized = normalizeWakePhrase(phrase);
    for (const QString &greeting : {QStringLiteral("HEY "), QStringLiteral("HELLO "), QStringLiteral("HI ")}) {
        if (normalized.startsWith(greeting)) {
            return normalized.mid(greeting.size()).trimmed();
        }
    }

    return normalized;
}

QList<WakeKeywordVariant> wakePhraseVariants(const QString &phrase, float baseThreshold)
{
    const QString normalized = normalizeWakePhrase(phrase);
    const QString baseTarget = stripWakeGreeting(normalized);
    const QString canonicalTarget = baseTarget.isEmpty() ? QStringLiteral("VAXIL") : baseTarget;
    const QString primaryPhrase = normalized.isEmpty() ? QStringLiteral("HEY VAXIL") : normalized;
    const float exactThreshold = std::clamp(baseThreshold - 0.07f, 0.10f, 0.40f);
    const float alternateThreshold = std::clamp(baseThreshold - 0.04f, 0.10f, 0.42f);
    const float bareThreshold = std::clamp(baseThreshold + 0.01f, 0.10f, 0.48f);

    QList<WakeKeywordVariant> variants;
    auto addVariant = [&](const QString &candidate, float boostingScore, float triggerThreshold) {
        const QString normalizedCandidate = normalizeWakePhrase(candidate);
        if (normalizedCandidate.isEmpty()) {
            return;
        }

        for (const WakeKeywordVariant &existing : variants) {
            if (existing.phrase == normalizedCandidate) {
                return;
            }
        }

        variants.push_back(WakeKeywordVariant{
            .phrase = normalizedCandidate,
            .boostingScore = boostingScore,
            .triggerThreshold = triggerThreshold
        });
    };

    addVariant(primaryPhrase, 2.2f, exactThreshold);

    const bool isVaxilFamily = canonicalTarget.contains(QStringLiteral("VAXIL"))
        || canonicalTarget.contains(QStringLiteral("VAKSIL"))
        || canonicalTarget.contains(QStringLiteral("VAXEL"));
    if (isVaxilFamily) {
        for (const QString &target : {QStringLiteral("VAXIL"), QStringLiteral("VAKSIL"), QStringLiteral("VAXEL")}) {
            addVariant(QStringLiteral("HEY %1").arg(target), target == canonicalTarget ? 2.2f : 1.9f, exactThreshold);
            addVariant(QStringLiteral("HELLO %1").arg(target), 1.7f, alternateThreshold);
            addVariant(QStringLiteral("HI %1").arg(target), 1.7f, alternateThreshold);
            addVariant(target, 1.3f, bareThreshold);
        }
    } else {
        addVariant(QStringLiteral("HEY %1").arg(canonicalTarget), 2.0f, exactThreshold);
        addVariant(QStringLiteral("HELLO %1").arg(canonicalTarget), 1.6f, alternateThreshold);
        addVariant(QStringLiteral("HI %1").arg(canonicalTarget), 1.6f, alternateThreshold);
        addVariant(canonicalTarget, 1.25f, bareThreshold);
    }

    return variants;
}

float mapWakeThreshold(float value)
{
    // New settings store "sensitivity" in a 0.5..1.0 range. The bundled Sherpa KWS
    // model expects a lower keyword threshold where smaller numbers are easier to fire.
    if (value >= 0.50f) {
        const float normalized = std::clamp((value - 0.50f) / 0.50f, 0.0f, 1.0f);
        return 0.10f + (normalized * 0.15f); // 0.80 -> 0.190, which is more realistic for the English KWS bundle.
    }

    return value;
}
}

SherpaWakeWordEngine::SherpaWakeWordEngine(AppSettings *settings, LoggingService *loggingService, QObject *parent)
    : WakeWordEngine(parent)
    , m_settings(settings)
    , m_loggingService(loggingService)
{
}

SherpaWakeWordEngine::~SherpaWakeWordEngine()
{
    stop();
}

bool SherpaWakeWordEngine::start(
    const QString &enginePath,
    const QString &modelPath,
    float threshold,
    int cooldownMs,
    const QString &preferredDeviceId)
{
    stop();

#if !JARVIS_HAS_SHERPA_ONNX
    Q_UNUSED(enginePath);
    Q_UNUSED(modelPath);
    Q_UNUSED(threshold);
    Q_UNUSED(cooldownMs);
    Q_UNUSED(preferredDeviceId);
    emit errorOccurred(QStringLiteral("sherpa-onnx support is not compiled into this build"));
    return false;
#else
    if (!QFileInfo::exists(enginePath)) {
        emit errorOccurred(QStringLiteral("sherpa-onnx runtime package is missing"));
        return false;
    }
    if (!QFileInfo::exists(modelPath)) {
        emit errorOccurred(QStringLiteral("sherpa-onnx keyword spotting model is missing"));
        return false;
    }

    m_threshold = std::clamp(mapWakeThreshold(threshold), 0.10f, 0.85f);
    m_cooldownMs = std::clamp(cooldownMs, kMinCooldownMs, kMaxCooldownMs);
    m_preferredDeviceId = preferredDeviceId;
    m_runtimeRoot = QFileInfo(enginePath).absoluteFilePath();
    m_modelRoot = QFileInfo(modelPath).absoluteFilePath();
    m_helperPath = resolveHelperExecutablePath();
    m_encoderPath = resolveModelFile(m_modelRoot, {
        QStringLiteral("encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx"),
        QStringLiteral("encoder-epoch-12-avg-2-chunk-16-left-64.onnx")
    });
    m_decoderPath = resolveModelFile(m_modelRoot, {
        QStringLiteral("decoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx"),
        QStringLiteral("decoder-epoch-12-avg-2-chunk-16-left-64.onnx")
    });
    m_joinerPath = resolveModelFile(m_modelRoot, {
        QStringLiteral("joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx"),
        QStringLiteral("joiner-epoch-12-avg-2-chunk-16-left-64.onnx")
    });
    m_tokensPath = QDir(m_modelRoot).filePath(QStringLiteral("tokens.txt"));

    if (m_helperPath.isEmpty()) {
        emit errorOccurred(QStringLiteral("Wake helper executable is missing"));
        return false;
    }
    if (m_encoderPath.isEmpty() || m_decoderPath.isEmpty() || m_joinerPath.isEmpty() || !QFileInfo::exists(m_tokensPath)) {
        emit errorOccurred(QStringLiteral("sherpa-onnx keyword model files are incomplete"));
        return false;
    }

    QString errorText;
    if (!writeKeywordsFile(m_modelRoot, &m_keywordsFilePath, &errorText)) {
        emit errorOccurred(errorText);
        return false;
    }

    m_paused = false;
    return startHelperProcess();
#endif
}

void SherpaWakeWordEngine::pause()
{
    if (!isActive() || m_paused) {
        return;
    }

    m_paused = true;
    m_ready = false;
    m_stopRequested = true;
    if (m_helperProcess) {
        m_helperProcess->terminate();
        if (!m_helperProcess->waitForFinished(500)) {
            m_helperProcess->kill();
        }
    }
    if (m_loggingService) {
        m_loggingService->infoFor(QStringLiteral("wake_engine"), QStringLiteral("sherpa-onnx wake detection paused."));
    }
}

void SherpaWakeWordEngine::resume()
{
    if (!m_paused || m_runtimeRoot.isEmpty() || m_modelRoot.isEmpty()) {
        return;
    }

    m_paused = false;
    startHelperProcess();
}

void SherpaWakeWordEngine::stop()
{
    m_stopRequested = true;
    m_paused = false;
    m_ready = false;

    if (m_helperProcess) {
        m_helperProcess->terminate();
        if (!m_helperProcess->waitForFinished(500)) {
            m_helperProcess->kill();
            m_helperProcess->waitForFinished(500);
        }
        m_helperProcess->deleteLater();
    }

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_runtimeRoot.clear();
    m_modelRoot.clear();
    m_keywordsFilePath.clear();
    m_encoderPath.clear();
    m_decoderPath.clear();
    m_joinerPath.clear();
    m_tokensPath.clear();
    m_helperPath.clear();
    m_helperProcess = nullptr;
    m_stopRequested = false;
}

bool SherpaWakeWordEngine::isActive() const
{
    if (m_paused && !m_runtimeRoot.isEmpty() && !m_modelRoot.isEmpty()) {
        return true;
    }

    return m_helperProcess != nullptr && m_helperProcess->state() != QProcess::NotRunning;
}

bool SherpaWakeWordEngine::isPaused() const
{
    return m_paused;
}

bool SherpaWakeWordEngine::startHelperProcess()
{
    if (m_helperPath.isEmpty()) {
        emit errorOccurred(QStringLiteral("Wake helper executable is missing"));
        return false;
    }

    if (m_helperProcess) {
        m_stopRequested = true;
        m_helperProcess->kill();
        m_helperProcess->deleteLater();
        m_helperProcess = nullptr;
    }

    m_ready = false;
    m_stopRequested = false;
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    QFile::remove(helperLogPath(QStringLiteral("wake_helper_stdout.txt")));
    QFile::remove(helperLogPath(QStringLiteral("wake_helper_stderr.txt")));

    m_helperProcess = new QProcess(this);
    connect(m_helperProcess, &QProcess::readyReadStandardOutput, this, &SherpaWakeWordEngine::consumeHelperStdout);
    connect(m_helperProcess, &QProcess::readyReadStandardError, this, &SherpaWakeWordEngine::consumeHelperStderr);
    connect(m_helperProcess, &QProcess::finished, this, &SherpaWakeWordEngine::handleHelperFinished);
    connect(m_helperProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_helperProcess) {
            return;
        }
        if (m_stopRequested || m_paused) {
            return;
        }
        if (error == QProcess::Crashed && !m_ready) {
            return;
        }

        const QString message = m_helperProcess->errorString().trimmed().isEmpty()
            ? QStringLiteral("Wake helper failed to start")
            : m_helperProcess->errorString().trimmed();
        m_ready = false;
        emit errorOccurred(message);
    });

    QStringList args{
        QStringLiteral("--encoder"), m_encoderPath,
        QStringLiteral("--decoder"), m_decoderPath,
        QStringLiteral("--joiner"), m_joinerPath,
        QStringLiteral("--tokens"), m_tokensPath,
        QStringLiteral("--keywords-file"), m_keywordsFilePath,
        QStringLiteral("--threshold"), QString::number(m_threshold, 'f', 3),
        QStringLiteral("--cooldown-ms"), QString::number(m_cooldownMs),
        QStringLiteral("--warmup-ms"), QString::number(m_activationWarmupMs)
    };
    if (!m_preferredDeviceId.trimmed().isEmpty()) {
        args << QStringLiteral("--device-id") << m_preferredDeviceId.trimmed();
    }

    m_helperProcess->setProgram(m_helperPath);
    m_helperProcess->setArguments(args);
    m_helperProcess->setWorkingDirectory(QFileInfo(m_helperPath).absolutePath());

    QStringList runtimeSearchPaths;
    const QFileInfo runtimeInfo(m_runtimeRoot);
    const QString runtimeBaseDir = runtimeInfo.isFile()
        ? runtimeInfo.absolutePath()
        : runtimeInfo.absoluteFilePath();
    if (!runtimeBaseDir.isEmpty()) {
        runtimeSearchPaths << runtimeBaseDir
                           << (runtimeBaseDir + QStringLiteral("/lib"))
                           << (runtimeBaseDir + QStringLiteral("/bin"));
    }
    runtimeSearchPaths << QFileInfo(m_helperPath).absolutePath();

    QStringList existingRuntimeDirs;
    existingRuntimeDirs.reserve(runtimeSearchPaths.size());
    for (const QString &candidate : std::as_const(runtimeSearchPaths)) {
        if (!candidate.isEmpty() && QFileInfo(candidate).exists()) {
            existingRuntimeDirs.push_back(QFileInfo(candidate).absoluteFilePath());
        }
    }
    existingRuntimeDirs.removeDuplicates();

    if (!existingRuntimeDirs.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#if defined(Q_OS_WIN)
        const QString existingPath = env.value(QStringLiteral("PATH"));
        env.insert(
            QStringLiteral("PATH"),
            existingRuntimeDirs.join(QStringLiteral(";"))
                + (existingPath.isEmpty() ? QString() : QStringLiteral(";") + existingPath));
#else
        const QString existingLdLibraryPath = env.value(QStringLiteral("LD_LIBRARY_PATH"));
        env.insert(
            QStringLiteral("LD_LIBRARY_PATH"),
            existingRuntimeDirs.join(QStringLiteral(":"))
                + (existingLdLibraryPath.isEmpty() ? QString() : QStringLiteral(":") + existingLdLibraryPath));
#endif
        m_helperProcess->setProcessEnvironment(env);
    }

    m_helperProcess->start();
    if (!m_helperProcess->waitForStarted(3000)) {
        const QString message = m_helperProcess->errorString().trimmed().isEmpty()
            ? QStringLiteral("Failed to start the sherpa wake helper")
            : m_helperProcess->errorString().trimmed();
        emit errorOccurred(message);
        m_helperProcess->deleteLater();
        m_helperProcess = nullptr;
        return false;
    }

    if (m_loggingService) {
        m_loggingService->infoFor(QStringLiteral("wake_engine"), QStringLiteral("[VAXIL] Starting sherpa wake helper. helper=\"%1\" model=\"%2\" kwsThreshold=%3 cooldownMs=%4 wakeWord=\"%5\" keywords=\"%6\"")
            .arg(m_helperPath, m_modelRoot)
            .arg(m_threshold, 0, 'f', 3)
            .arg(m_cooldownMs)
            .arg(m_settings ? m_settings->wakeWordPhrase() : QStringLiteral("Hey Vaxil"))
            .arg(m_keywordsFilePath));
    }
    return true;
}

void SherpaWakeWordEngine::consumeHelperStdout()
{
    if (!m_helperProcess) {
        return;
    }

    m_stdoutBuffer.append(m_helperProcess->readAllStandardOutput());
    qsizetype newlineIndex = m_stdoutBuffer.indexOf('\n');
    while (newlineIndex >= 0) {
        QByteArray line = m_stdoutBuffer.left(newlineIndex).trimmed();
        m_stdoutBuffer.remove(0, newlineIndex + 1);
        appendHelperLogLine(QStringLiteral("wake_helper_stdout.txt"), line);

        const QString text = QString::fromUtf8(line);
        if (text == QStringLiteral("READY")) {
            m_ready = true;
            if (m_loggingService) {
                m_loggingService->infoFor(QStringLiteral("wake_engine"), QStringLiteral("[VAXIL] Sherpa wake engine started. runtime=\"%1\" model=\"%2\" kwsThreshold=%3 cooldownMs=%4 wakeWord=\"%5\"")
                    .arg(m_runtimeRoot, m_modelRoot)
                    .arg(m_threshold, 0, 'f', 3)
                    .arg(m_cooldownMs)
                    .arg(m_settings ? m_settings->wakeWordPhrase() : QStringLiteral("Hey Vaxil")));
            }
            emit engineReady();
        } else if (text.startsWith(QStringLiteral("DETECTED:"), Qt::CaseInsensitive)) {
            if (!m_paused && m_ready) {
                if (m_loggingService) {
                    m_loggingService->infoFor(QStringLiteral("wake_engine"), QStringLiteral("[VAXIL] Wake word detected. keyword=\"%1\"")
                        .arg(text.mid(QStringLiteral("DETECTED:").size()).trimmed()));
                }
                emit probabilityUpdated(1.0f);
                emit wakeWordDetected();
            }
        } else if (text.startsWith(QStringLiteral("ERROR:"), Qt::CaseInsensitive)) {
            const QString message = text.mid(QStringLiteral("ERROR:").size()).trimmed();
            if (!message.isEmpty()) {
                emit errorOccurred(message);
            }
        } else if (!text.isEmpty() && m_loggingService) {
            m_loggingService->infoFor(QStringLiteral("wake_engine"), QStringLiteral("sherpa wake helper: %1").arg(text));
        }

        newlineIndex = m_stdoutBuffer.indexOf('\n');
    }
}

void SherpaWakeWordEngine::consumeHelperStderr()
{
    if (!m_helperProcess) {
        return;
    }

    m_stderrBuffer.append(m_helperProcess->readAllStandardError());
    qsizetype newlineIndex = m_stderrBuffer.indexOf('\n');
    while (newlineIndex >= 0) {
        QByteArray line = m_stderrBuffer.left(newlineIndex).trimmed();
        m_stderrBuffer.remove(0, newlineIndex + 1);
        appendHelperLogLine(QStringLiteral("wake_helper_stderr.txt"), line);
        const QString text = QString::fromUtf8(line);
        if (!text.isEmpty() && m_loggingService) {
            m_loggingService->warnFor(QStringLiteral("wake_engine"), QStringLiteral("sherpa wake helper stderr: %1").arg(text));
        }
        newlineIndex = m_stderrBuffer.indexOf('\n');
    }
}

void SherpaWakeWordEngine::handleHelperFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const bool intentionalStop = m_stopRequested || m_paused;
    const bool unexpected = !m_stopRequested && !m_paused;
    m_ready = false;

    if (unexpected) {
        QString message = QStringLiteral("Wake helper exited unexpectedly");
        if (!m_stderrBuffer.trimmed().isEmpty()) {
            message = QString::fromUtf8(m_stderrBuffer).trimmed();
        } else if (m_helperProcess && !m_helperProcess->errorString().trimmed().isEmpty()) {
            message = m_helperProcess->errorString().trimmed();
        } else {
            message += QStringLiteral(" (exitCode=%1, status=%2)")
                .arg(exitCode)
                .arg(exitStatus == QProcess::NormalExit ? QStringLiteral("normal") : QStringLiteral("crash"));
        }
        emit errorOccurred(message);
    }

    if (m_helperProcess) {
        m_helperProcess->deleteLater();
    }
    m_helperProcess = nullptr;
    if (intentionalStop) {
        m_stderrBuffer.clear();
    }
    m_stopRequested = false;
}

QString SherpaWakeWordEngine::resolveHelperExecutablePath() const
{
    const QString helperName = PlatformRuntime::helperExecutableName(QStringLiteral("vaxil_wake_helper"));
    const QString legacyHelperName = PlatformRuntime::helperExecutableName(QStringLiteral("jarvis_sherpa_wake_helper"));
    return firstExisting({
        QCoreApplication::applicationDirPath() + QStringLiteral("/") + helperName,
        QCoreApplication::applicationDirPath() + QStringLiteral("/") + legacyHelperName,
        QStringLiteral(JARVIS_SOURCE_DIR) + QStringLiteral("/bin/") + helperName,
        QStringLiteral(JARVIS_SOURCE_DIR) + QStringLiteral("/bin/") + legacyHelperName,
        QStringLiteral(JARVIS_SOURCE_DIR) + QStringLiteral("/build-release/") + helperName,
        QStringLiteral(JARVIS_SOURCE_DIR) + QStringLiteral("/build-release/") + legacyHelperName,
        QStringLiteral(JARVIS_SOURCE_DIR) + QStringLiteral("/build/") + helperName,
        QStringLiteral(JARVIS_SOURCE_DIR) + QStringLiteral("/build/") + legacyHelperName
    });
}

QString SherpaWakeWordEngine::resolveModelFile(const QString &rootPath, const QStringList &fileNames) const
{
    QStringList candidates;
    for (const QString &fileName : fileNames) {
        candidates.push_back(QDir(rootPath).filePath(fileName));
    }
    return firstExisting(candidates);
}

QString SherpaWakeWordEngine::helperLogPath(const QString &fileName) const
{
    const QString logsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/logs");
    QDir().mkpath(logsDir);
    return logsDir + QStringLiteral("/") + fileName;
}

void SherpaWakeWordEngine::appendHelperLogLine(const QString &fileName, const QByteArray &line) const
{
    QFile file(helperLogPath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }

    file.write(line);
    file.write("\n");
}

bool SherpaWakeWordEngine::writeKeywordsFile(const QString &modelPath, QString *keywordsPath, QString *errorText) const
{
#if !JARVIS_HAS_SHERPA_ONNX
    Q_UNUSED(modelPath);
    Q_UNUSED(keywordsPath);
    if (errorText) {
        *errorText = QStringLiteral("sentencepiece support is unavailable");
    }
    return false;
#else
    const QString bpeModel = QDir(modelPath).filePath(QStringLiteral("bpe.model"));
    if (!QFileInfo::exists(bpeModel)) {
        if (errorText) {
            *errorText = QStringLiteral("sherpa-onnx BPE model is missing");
        }
        return false;
    }

    sentencepiece::SentencePieceProcessor processor;
    const auto loadStatus = processor.Load(bpeModel.toStdString());
    if (!loadStatus.ok()) {
        if (errorText) {
            *errorText = QStringLiteral("Failed to load sentencepiece wake model: %1")
                .arg(QString::fromStdString(loadStatus.ToString()));
        }
        return false;
    }

    const QString displayPhrase = (m_settings && !m_settings->wakeWordPhrase().trimmed().isEmpty())
        ? m_settings->wakeWordPhrase().trimmed()
        : QStringLiteral("Hey Vaxil");
    const QString keywordLabel = normalizeWakePhrase(displayPhrase).replace(QChar::fromLatin1(' '), QChar::fromLatin1('_'));

    QStringList lines;
    QSet<QString> emittedLines;
    for (const WakeKeywordVariant &variant : wakePhraseVariants(displayPhrase, m_threshold)) {
        const std::string variantUtf8 = variant.phrase.toStdString();
        std::vector<std::string> pieces;
        const auto encodeStatus = processor.Encode(variantUtf8, &pieces);
        if (!encodeStatus.ok() || pieces.empty()) {
            if (errorText) {
                *errorText = QStringLiteral("Failed to encode wake phrase \"%1\": %2")
                    .arg(variant.phrase, QString::fromStdString(encodeStatus.ToString()));
            }
            return false;
        }

        auto addKeywordPieces = [&](const std::vector<std::string> &keywordPieces, float boostingScore, float triggerThreshold) {
            if (keywordPieces.empty()) {
                return;
            }

            QStringList linePieces;
            linePieces.reserve(static_cast<qsizetype>(keywordPieces.size()));
            for (const std::string &piece : keywordPieces) {
                linePieces.push_back(QString::fromStdString(piece));
            }

            const QString line = QStringLiteral("%1 :%2 #%3 @%4")
                .arg(linePieces.join(QChar::fromLatin1(' ')))
                .arg(boostingScore, 0, 'f', 2)
                .arg(triggerThreshold, 0, 'f', 2)
                .arg(keywordLabel);
            if (!emittedLines.contains(line)) {
                emittedLines.insert(line);
                lines.push_back(line);
            }
        };

        addKeywordPieces(pieces, variant.boostingScore, variant.triggerThreshold);

        std::vector<std::vector<std::string>> nbestPieces;
        const auto nbestStatus = processor.NBestEncode(variantUtf8, kKeywordSegmentationCandidates, &nbestPieces);
        if (nbestStatus.ok()) {
            for (const auto &candidate : nbestPieces) {
                addKeywordPieces(candidate, variant.boostingScore, variant.triggerThreshold);
            }
        }
    }

    if (lines.isEmpty()) {
        if (errorText) {
            *errorText = QStringLiteral("No encoded keyword variants were generated for the wake phrase");
        }
        return false;
    }

    const QString wakeDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/wake");
    QDir().mkpath(wakeDir);
    const QString outputPath = wakeDir + QStringLiteral("/vaxil_keywords.txt");
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorText) {
            *errorText = QStringLiteral("Failed to write sherpa-onnx keywords file");
        }
        return false;
    }

    file.write(lines.join(QChar::fromLatin1('\n')).toUtf8());
    file.write("\n");
    file.close();

    if (keywordsPath) {
        *keywordsPath = outputPath;
    }

    return true;
#endif
}
