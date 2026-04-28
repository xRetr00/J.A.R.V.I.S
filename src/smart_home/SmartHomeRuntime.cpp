#include "smart_home/SmartHomeRuntime.h"

#include <QDateTime>
#include <QMutex>
#include <QRegularExpression>
#include <QTimer>

#include "logging/LoggingService.h"
#include "settings/AppSettings.h"
#include "smart_home/DesktopBleIdentityAdapter.h"
#include "smart_home/HomeAssistantSmartHomeAdapter.h"

namespace {
QMutex &sharedSnapshotMutex()
{
    static QMutex mutex;
    return mutex;
}

SmartHomeSnapshot &sharedSnapshot()
{
    static SmartHomeSnapshot snapshot;
    return snapshot;
}

QString safeTokenEnvVarForLog(QString value)
{
    value = value.trimmed();
    static const QRegularExpression envNamePattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (envNamePattern.match(value).hasMatch()) {
        return value;
    }
    return QStringLiteral("VAXIL_HOME_ASSISTANT_TOKEN");
}
}

SmartHomeRuntime::SmartHomeRuntime(AppSettings *settings, LoggingService *loggingService, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_loggingService(loggingService)
    , m_pollTimer(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &SmartHomeRuntime::pollState);
    if (m_settings) {
        connect(m_settings, &AppSettings::settingsChanged, this, &SmartHomeRuntime::reconfigureFromSettings);
    }
}

void SmartHomeRuntime::start()
{
    reconfigureFromSettings();
}

void SmartHomeRuntime::stop()
{
    m_pollTimer->stop();
    if (m_identityAdapter) {
        m_identityAdapter->stop();
    }
    m_pollInFlight = false;
}

SmartHomeSnapshot SmartHomeRuntime::latestSnapshot() const
{
    return m_latestSnapshot;
}

SmartHomeSnapshot SmartHomeRuntime::latestSharedSnapshot()
{
    QMutexLocker locker(&sharedSnapshotMutex());
    return sharedSnapshot();
}

void SmartHomeRuntime::reconfigureFromSettings()
{
    m_config = configFromSettings();
    if (m_adapter == nullptr) {
        m_adapter = new HomeAssistantSmartHomeAdapter(m_config, this);
    } else {
        m_adapter->setConfig(m_config);
    }
    reconfigureIdentityAdapter();

    m_pollTimer->setInterval(std::max(1000, m_config.pollIntervalMs));
    if (!m_config.enabled) {
        stop();
        emit statusChanged(QStringLiteral("Smart room disabled"), false);
        return;
    }

    if (m_loggingService) {
        m_loggingService->infoFor(
            QStringLiteral("tools_mcp"),
            QStringLiteral("[smart_home] provider=%1 enabled=true baseUrlConfigured=%2 presenceEntity=%3 lightEntity=%4 pollIntervalMs=%5 tokenEnv=%6")
                .arg(m_config.provider,
                     m_config.homeAssistantBaseUrl.trimmed().isEmpty() ? QStringLiteral("false") : QStringLiteral("true"),
                     m_config.presenceEntityId,
                     m_config.lightEntityId,
                     QString::number(m_config.pollIntervalMs),
                     safeTokenEnvVarForLog(m_config.homeAssistantTokenEnvVar)));
    }

    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
    pollState();
}

void SmartHomeRuntime::pollState()
{
    if (!m_config.enabled || m_adapter == nullptr || m_pollInFlight) {
        return;
    }
    m_pollInFlight = true;
    m_adapter->fetchState([this](const SmartHomeSnapshot &snapshot) {
        m_pollInFlight = false;
        handleSnapshot(snapshot);
    });
}

SmartHomeConfig SmartHomeRuntime::configFromSettings() const
{
    SmartHomeConfig config;
    if (!m_settings) {
        return config;
    }
    config.enabled = m_settings->smartHomeEnabled();
    config.provider = m_settings->smartHomeProvider();
    config.homeAssistantBaseUrl = m_settings->smartHomeHomeAssistantBaseUrl();
    config.homeAssistantTokenEnvVar = m_settings->smartHomeHomeAssistantTokenEnvVar();
    config.presenceEntityId = m_settings->smartHomePresenceEntityId();
    config.lightEntityId = m_settings->smartHomeLightEntityId();
    config.identityMode = m_settings->smartHomeIdentityMode();
    config.bleBeaconUuid = m_settings->smartHomeBleBeaconUuid();
    config.pollIntervalMs = m_settings->smartHomePollIntervalMs();
    config.sensorOnlyWelcomeEnabled = m_settings->smartHomeSensorOnlyWelcomeEnabled();
    config.welcomeCooldownMinutes = m_settings->smartHomeWelcomeCooldownMinutes();
    config.roomAbsenceGraceMinutes = m_settings->smartHomeRoomAbsenceGraceMinutes();
    config.requestTimeoutMs = m_settings->smartHomeRequestTimeoutMs();
    config.bleAwayTimeoutMinutes = m_settings->smartHomeBleMissingTimeoutMinutes();
    config.bleScanIntervalMs = m_settings->smartHomeBleScanIntervalMs();
    config.bleRssiThreshold = m_settings->smartHomeBleRssiThreshold();
    return config;
}

void SmartHomeRuntime::handleSnapshot(const SmartHomeSnapshot &snapshot)
{
    SmartHomeSnapshot enrichedSnapshot = snapshot;
    if (m_loggingService) {
        m_loggingService->infoFor(
            QStringLiteral("tools_mcp"),
            QStringLiteral("[smart_home.poll] success=%1 httpStatus=%2 latencyMs=%3 presenceAvailable=%4 occupied=%5 lightAvailable=%6 lightOn=%7")
                .arg(snapshot.success ? QStringLiteral("true") : QStringLiteral("false"),
                     QString::number(snapshot.httpStatus),
                     QString::number(snapshot.latencyMs),
                     snapshot.presence.available ? QStringLiteral("true") : QStringLiteral("false"),
                     snapshot.presence.occupied ? QStringLiteral("true") : QStringLiteral("false"),
                     snapshot.light.available ? QStringLiteral("true") : QStringLiteral("false"),
                     snapshot.light.on ? QStringLiteral("true") : QStringLiteral("false")));
    }

    if (!snapshot.success) {
        m_latestSnapshot = enrichedSnapshot;
        emit statusChanged(snapshot.detail.trimmed().isEmpty() ? QStringLiteral("Smart room unavailable") : snapshot.detail, false);
        return;
    }

    emit statusChanged(QStringLiteral("Smart room connected"), true);
    SmartRoomStateMachineConfig stateConfig;
    stateConfig.sensorOnlyWelcomeEnabled = m_config.sensorOnlyWelcomeEnabled;
    stateConfig.roomAbsenceGraceMinutes = m_config.roomAbsenceGraceMinutes;
    stateConfig.bleAwayTimeoutMinutes = m_config.bleAwayTimeoutMinutes;
    const std::optional<BleIdentitySnapshot> identity = m_identityAdapter
        ? m_identityAdapter->latestIdentityPresence()
        : std::nullopt;
    const SmartRoomTransition transition = m_stateMachine.evaluate({
        .presence = snapshot.presence,
        .identity = identity,
        .config = stateConfig,
        .nowMs = QDateTime::currentMSecsSinceEpoch()
    });
    enrichedSnapshot.identity = identity;
    enrichedSnapshot.roomState = transition.currentState;
    enrichedSnapshot.roomReasonCode = transition.reasonCode;
    m_latestSnapshot = enrichedSnapshot;
    {
        QMutexLocker locker(&sharedSnapshotMutex());
        sharedSnapshot() = enrichedSnapshot;
    }
    logTransition(transition);
    emit roomTransitionReady(transition);

    const bool identityAvailable = identity.has_value() && identity->available && !identity->stale;
    const SmartWelcomeDecision welcome = m_behaviorPolicy.evaluateWelcome({
        .transition = transition,
        .sensorOnlyWelcomeEnabled = identityAvailable ? false : m_config.sensorOnlyWelcomeEnabled,
        .welcomeCooldownMinutes = m_config.welcomeCooldownMinutes,
        .lastWelcomeAtMs = m_lastWelcomeAtMs,
        .nowMs = transition.occurredAtMs
    });
    logWelcomeDecision(welcome, transition);
    if (welcome.allowed) {
        m_lastWelcomeAtMs = welcome.nextLastWelcomeAtMs;
        emit welcomeRequested(welcome.message, welcome);
    }
}

void SmartHomeRuntime::reconfigureIdentityAdapter()
{
    if (m_config.identityMode == QStringLiteral("desktop_ble_beacon")) {
        if (m_identityAdapter == nullptr) {
            m_identityAdapter = new DesktopBleIdentityAdapter(
                m_config,
                [this](const QString &message) {
                    if (m_loggingService) {
                        m_loggingService->infoFor(QStringLiteral("tools_mcp"), message);
                    }
                },
                this);
        } else {
            m_identityAdapter->setConfig(m_config);
        }
        if (m_config.enabled) {
            m_identityAdapter->start();
        }
        return;
    }

    if (m_identityAdapter) {
        m_identityAdapter->stop();
        m_identityAdapter->deleteLater();
        m_identityAdapter = nullptr;
    }
}

void SmartHomeRuntime::logTransition(const SmartRoomTransition &transition) const
{
    if (!m_loggingService) {
        return;
    }
    m_loggingService->infoFor(
        QStringLiteral("tools_mcp"),
        QStringLiteral("[smart_room.transition] previous=%1 current=%2 reason=%3 roomOccupied=%4 phonePresent=%5 sensorOffGapMs=%6 bleMissingMs=%7")
            .arg(smartRoomOccupancyStateName(transition.previousState),
                 smartRoomOccupancyStateName(transition.currentState),
                 transition.reasonCode,
                 transition.roomOccupied ? QStringLiteral("true") : QStringLiteral("false"),
                 transition.phonePresent ? QStringLiteral("true") : QStringLiteral("false"),
                 QString::number(transition.sensorOffGapMs),
                 QString::number(transition.bleMissingMs)));
}

void SmartHomeRuntime::logWelcomeDecision(const SmartWelcomeDecision &decision, const SmartRoomTransition &transition) const
{
    if (!m_loggingService) {
        return;
    }
    m_loggingService->infoFor(
        QStringLiteral("tools_mcp"),
        QStringLiteral("[smart_room.welcome] allowed=%1 reason=%2 previous=%3 current=%4 cooldownMinutes=%5 sensorOnlyEnabled=%6")
            .arg(decision.allowed ? QStringLiteral("true") : QStringLiteral("false"),
                 decision.reasonCode,
                 smartRoomOccupancyStateName(transition.previousState),
                 smartRoomOccupancyStateName(transition.currentState),
                 QString::number(m_config.welcomeCooldownMinutes),
                 m_config.sensorOnlyWelcomeEnabled ? QStringLiteral("true") : QStringLiteral("false")));
}
