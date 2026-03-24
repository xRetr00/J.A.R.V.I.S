#include "wakeword/PorcupineWakeWordEngine.h"

#include <QFileInfo>
#include <QIODevice>
#include <QMediaDevices>

#include "logging/LoggingService.h"

namespace {
constexpr const char *kPorcupineDevice = "cpu:2";
}

PorcupineWakeWordEngine::PorcupineWakeWordEngine(LoggingService *loggingService, QObject *parent)
    : QObject(parent)
    , m_loggingService(loggingService)
{
}

PorcupineWakeWordEngine::~PorcupineWakeWordEngine()
{
    stop();
}

bool PorcupineWakeWordEngine::start(
    const QString &accessKey,
    const QString &libraryPath,
    const QString &modelPath,
    const QString &keywordPath,
    float sensitivity,
    const QString &preferredDeviceId)
{
    stop();

    if (accessKey.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Porcupine AccessKey is not configured"));
        return false;
    }
    if (!QFileInfo::exists(libraryPath)) {
        emit errorOccurred(QStringLiteral("Porcupine library is missing"));
        return false;
    }
    if (!QFileInfo::exists(modelPath)) {
        emit errorOccurred(QStringLiteral("Porcupine model file is missing"));
        return false;
    }
    if (!QFileInfo::exists(keywordPath)) {
        emit errorOccurred(QStringLiteral("Porcupine keyword file is missing"));
        return false;
    }

    if (!loadRuntime(libraryPath)) {
        emit errorOccurred(QStringLiteral("Unable to load Porcupine runtime"));
        return false;
    }

    const QByteArray accessKeyUtf8 = accessKey.trimmed().toUtf8();
    const QByteArray modelPathUtf8 = QFileInfo(modelPath).absoluteFilePath().toUtf8();
    const QByteArray keywordPathUtf8 = QFileInfo(keywordPath).absoluteFilePath().toUtf8();
    const char *keywordPaths[] = { keywordPathUtf8.constData() };
    const float sensitivities[] = { sensitivity };

    const pv_status_t initStatus = m_pvPorcupineInit(
        accessKeyUtf8.constData(),
        modelPathUtf8.constData(),
        kPorcupineDevice,
        1,
        keywordPaths,
        sensitivities,
        &m_handle);
    if (initStatus != PV_STATUS_SUCCESS || !m_handle) {
        const QString message = describeStatus(QStringLiteral("Porcupine initialization failed"), initStatus);
        if (m_loggingService) {
            m_loggingService->error(message);
        }
        unloadRuntime();
        emit errorOccurred(message);
        return false;
    }

    m_frameLength = m_pvPorcupineFrameLength();
    m_format.setSampleRate(m_pvSampleRate());
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!preferredDeviceId.isEmpty()) {
        const auto inputs = QMediaDevices::audioInputs();
        for (const QAudioDevice &candidate : inputs) {
            if (QString::fromUtf8(candidate.id()) == preferredDeviceId) {
                device = candidate;
                break;
            }
        }
    }

    if (device.isNull()) {
        stop();
        emit errorOccurred(QStringLiteral("No microphone available for Porcupine"));
        return false;
    }

    if (!device.isFormatSupported(m_format)) {
        const QString message = QStringLiteral("Microphone does not support the required Porcupine format (16 kHz mono PCM)");
        if (m_loggingService) {
            m_loggingService->error(message);
        }
        stop();
        emit errorOccurred(message);
        return false;
    }

    m_audioSource = std::make_unique<QAudioSource>(device, m_format, this);
    m_audioSource->setBufferSize(m_frameLength * static_cast<int>(sizeof(int16_t)) * 4);
    m_ioDevice = m_audioSource->start();
    if (!m_ioDevice) {
        stop();
        emit errorOccurred(QStringLiteral("Failed to start Porcupine microphone capture"));
        return false;
    }

    m_readyReadConnection = connect(m_ioDevice, &QIODevice::readyRead, this, &PorcupineWakeWordEngine::processBuffer);
    if (m_loggingService) {
        m_loggingService->info(QStringLiteral("Porcupine wake engine started. library=\"%1\" model=\"%2\" keyword=\"%3\" frameLength=%4 sampleRate=%5")
            .arg(libraryPath, modelPath, keywordPath)
            .arg(m_frameLength)
            .arg(m_format.sampleRate()));
    }
    return true;
}

void PorcupineWakeWordEngine::stop()
{
    if (m_readyReadConnection) {
        disconnect(m_readyReadConnection);
        m_readyReadConnection = {};
    }

    if (m_audioSource) {
        m_audioSource->stop();
        m_audioSource.reset();
    }
    m_ioDevice = nullptr;
    m_pendingPcm.clear();

    if (m_handle && m_pvPorcupineDelete) {
        m_pvPorcupineDelete(m_handle);
        m_handle = nullptr;
    }

    unloadRuntime();
}

bool PorcupineWakeWordEngine::isActive() const
{
    return m_audioSource != nullptr && m_handle != nullptr;
}

bool PorcupineWakeWordEngine::loadRuntime(const QString &libraryPath)
{
    unloadRuntime();

    m_runtimeLibrary.setFileName(QFileInfo(libraryPath).absoluteFilePath());
    if (!m_runtimeLibrary.load()) {
        if (m_loggingService) {
            m_loggingService->error(QStringLiteral("Failed to load Porcupine DLL: %1").arg(m_runtimeLibrary.errorString()));
        }
        return false;
    }

    m_pvSampleRate = reinterpret_cast<PvSampleRateFn>(m_runtimeLibrary.resolve("pv_sample_rate"));
    m_pvStatusToString = reinterpret_cast<PvStatusToStringFn>(m_runtimeLibrary.resolve("pv_status_to_string"));
    m_pvGetErrorStack = reinterpret_cast<PvGetErrorStackFn>(m_runtimeLibrary.resolve("pv_get_error_stack"));
    m_pvFreeErrorStack = reinterpret_cast<PvFreeErrorStackFn>(m_runtimeLibrary.resolve("pv_free_error_stack"));
    m_pvPorcupineInit = reinterpret_cast<PvPorcupineInitFn>(m_runtimeLibrary.resolve("pv_porcupine_init"));
    m_pvPorcupineDelete = reinterpret_cast<PvPorcupineDeleteFn>(m_runtimeLibrary.resolve("pv_porcupine_delete"));
    m_pvPorcupineProcess = reinterpret_cast<PvPorcupineProcessFn>(m_runtimeLibrary.resolve("pv_porcupine_process"));
    m_pvPorcupineFrameLength = reinterpret_cast<PvPorcupineFrameLengthFn>(m_runtimeLibrary.resolve("pv_porcupine_frame_length"));

    const bool resolved = m_pvSampleRate
        && m_pvStatusToString
        && m_pvPorcupineInit
        && m_pvPorcupineDelete
        && m_pvPorcupineProcess
        && m_pvPorcupineFrameLength;
    if (!resolved) {
        if (m_loggingService) {
            m_loggingService->error(QStringLiteral("Porcupine DLL is missing required exports"));
        }
        unloadRuntime();
        return false;
    }

    return true;
}

void PorcupineWakeWordEngine::unloadRuntime()
{
    m_pvSampleRate = nullptr;
    m_pvStatusToString = nullptr;
    m_pvGetErrorStack = nullptr;
    m_pvFreeErrorStack = nullptr;
    m_pvPorcupineInit = nullptr;
    m_pvPorcupineDelete = nullptr;
    m_pvPorcupineProcess = nullptr;
    m_pvPorcupineFrameLength = nullptr;
    m_frameLength = 0;

    if (m_runtimeLibrary.isLoaded()) {
        m_runtimeLibrary.unload();
    }
}

QString PorcupineWakeWordEngine::describeStatus(const QString &operation, pv_status_t status) const
{
    QString description = operation;
    if (m_pvStatusToString) {
        description += QStringLiteral(": %1").arg(QString::fromUtf8(m_pvStatusToString(status)));
    }

    if (m_pvGetErrorStack && m_pvFreeErrorStack) {
        char **messages = nullptr;
        int32_t depth = 0;
        const pv_status_t stackStatus = m_pvGetErrorStack(&messages, &depth);
        if (stackStatus == PV_STATUS_SUCCESS && messages != nullptr && depth > 0) {
            QStringList parts;
            for (int32_t i = 0; i < depth; ++i) {
                if (messages[i] != nullptr) {
                    parts << QString::fromUtf8(messages[i]);
                }
            }
            m_pvFreeErrorStack(messages);
            if (!parts.isEmpty()) {
                description += QStringLiteral(" [%1]").arg(parts.join(QStringLiteral(" | ")));
            }
        }
    }

    return description;
}

void PorcupineWakeWordEngine::processBuffer()
{
    if (!m_ioDevice || !m_handle || !m_pvPorcupineProcess || m_frameLength <= 0) {
        return;
    }

    m_pendingPcm.append(m_ioDevice->readAll());
    const int frameBytes = m_frameLength * static_cast<int>(sizeof(int16_t));
    while (m_pendingPcm.size() >= frameBytes) {
        const QByteArray frame = m_pendingPcm.left(frameBytes);
        m_pendingPcm.remove(0, frameBytes);

        int32_t keywordIndex = -1;
        const pv_status_t status = m_pvPorcupineProcess(
            m_handle,
            reinterpret_cast<const int16_t *>(frame.constData()),
            &keywordIndex);
        if (status != PV_STATUS_SUCCESS) {
            const QString message = describeStatus(QStringLiteral("Porcupine processing failed"), status);
            if (m_loggingService) {
                m_loggingService->error(message);
            }
            stop();
            emit errorOccurred(message);
            return;
        }

        if (keywordIndex >= 0) {
            if (m_loggingService) {
                m_loggingService->info(QStringLiteral("Porcupine wake word detected. keywordIndex=%1").arg(keywordIndex));
            }
            stop();
            emit wakeWordDetected();
            return;
        }
    }
}
