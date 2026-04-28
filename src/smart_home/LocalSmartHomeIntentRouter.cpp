#include "smart_home/LocalSmartHomeIntentRouter.h"

#include <algorithm>

#include <QDateTime>
#include <QRegularExpression>

#include "logging/LoggingService.h"
#include "settings/AppSettings.h"
#include "smart_home/HomeAssistantSmartHomeAdapter.h"
#include "smart_home/SmartHomeRuntime.h"

namespace {
QString normalizedInput(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QRegularExpression(QStringLiteral("[?!.,]")));
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return value;
}

bool containsAny(const QString &value, std::initializer_list<QStringView> needles)
{
    for (QStringView needle : needles) {
        if (value.contains(needle)) {
            return true;
        }
    }
    return false;
}

bool isComplexSmartHomeRequest(const QString &value)
{
    return containsAny(value,
                       {u"scene", u"movie", u"relaxing", u"if someone", u"when someone", u"notify", u"automation", u"routine"});
}

QString identityText(const SmartHomeSnapshot &snapshot)
{
    if (!snapshot.identity.has_value() || !snapshot.identity->available || snapshot.identity->stale) {
        return QStringLiteral("identity unavailable");
    }
    return snapshot.identity->present ? QStringLiteral("phone detected") : QStringLiteral("phone not detected");
}

QString localUnknownTime(const SmartRoomUnknownOccupantEvent &event)
{
    const qint64 timeMs = event.lastSeenAtUtcMs > 0 ? event.lastSeenAtUtcMs : event.firstDetectedAtUtcMs;
    if (timeMs <= 0) {
        return QStringLiteral("earlier");
    }
    return QDateTime::fromMSecsSinceEpoch(timeMs, Qt::UTC).toLocalTime().toString(QStringLiteral("HH:mm"));
}
}

LocalSmartHomeIntentRouter::LocalSmartHomeIntentRouter(AppSettings *settings,
                                                       LoggingService *loggingService,
                                                       QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_loggingService(loggingService)
{
}

bool LocalSmartHomeIntentRouter::handle(const QString &input, std::function<void(const LocalSmartHomeRouteResult &)> callback)
{
    const std::optional<LocalSmartHomeIntent> intent = match(input);
    if (!intent.has_value()) {
        return false;
    }

    const qint64 startedAtMs = QDateTime::currentMSecsSinceEpoch();
    if (intent->action == QStringLiteral("get_room_status")
        || intent->action == QStringLiteral("get_light_status")
        || intent->action == QStringLiteral("get_unknown_occupant_status")) {
        LocalSmartHomeRouteResult result = answerFromSnapshot(*intent, startedAtMs);
        logRoute(result);
        callback(result);
        return true;
    }

    auto *adapter = new HomeAssistantSmartHomeAdapter(configFromSettings(), this);
    SmartLightCommand command;
    command.action = intent->action;
    command.brightnessPercent = intent->brightnessPercent;
    command.colorName = intent->colorName;
    adapter->executeLightCommand(command, [this, adapter, action = intent->action, callback, startedAtMs](const SmartLightCommandResult &commandResult) {
        adapter->deleteLater();
        LocalSmartHomeRouteResult result;
        result.handled = true;
        result.success = commandResult.success;
        result.action = action;
        result.response = commandResult.summary.trimmed().isEmpty()
            ? (commandResult.success ? QStringLiteral("Done.") : QStringLiteral("Smart light is unavailable."))
            : commandResult.summary.trimmed();
        result.status = commandResult.success ? QStringLiteral("Smart light command") : QStringLiteral("Smart light unavailable");
        result.latencyMs = std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - startedAtMs);
        logRoute(result);
        callback(result);
    });
    return true;
}

SmartHomeConfig LocalSmartHomeIntentRouter::configFromSettings() const
{
    SmartHomeConfig config;
    if (!m_settings) {
        return config;
    }
    config.enabled = m_settings->smartHomeEnabled();
    config.provider = m_settings->smartHomeProvider();
    config.homeAssistantBaseUrl = m_settings->smartHomeHomeAssistantBaseUrl();
    config.homeAssistantTokenEnvVar = m_settings->smartHomeHomeAssistantTokenEnvVar();
    config.homeAssistantIdentityEntityId = m_settings->smartHomeHomeAssistantIdentityEntityId();
    config.presenceEntityId = m_settings->smartHomePresenceEntityId();
    config.lightEntityId = m_settings->smartHomeLightEntityId();
    config.identityMode = m_settings->smartHomeIdentityMode();
    config.pollIntervalMs = m_settings->smartHomePollIntervalMs();
    config.requestTimeoutMs = m_settings->smartHomeRequestTimeoutMs();
    return config;
}

LocalSmartHomeRouteResult LocalSmartHomeIntentRouter::answerFromSnapshot(const LocalSmartHomeIntent &intent, qint64 startedAtMs) const
{
    const SmartHomeSnapshot snapshot = SmartHomeRuntime::latestSharedSnapshot();
    LocalSmartHomeRouteResult result;
    result.handled = true;
    result.success = snapshot.success;
    result.action = intent.action;
    result.latencyMs = std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - startedAtMs);
    if (!snapshot.success) {
        result.status = QStringLiteral("Smart room unavailable");
        result.response = QStringLiteral("Smart room is unavailable.");
        return result;
    }
    result.status = QStringLiteral("Smart room status");
    if (intent.action == QStringLiteral("get_light_status")) {
        result.response = summarizeLightStatus(snapshot);
    } else if (intent.action == QStringLiteral("get_unknown_occupant_status")) {
        if (snapshot.unknownOccupant.active) {
            result.response = QStringLiteral("Yes, someone appears to be in the room now, but your phone is not home.");
        } else if (snapshot.unknownOccupant.hasEvent && !snapshot.unknownOccupant.acknowledged) {
            result.response = QStringLiteral("Yes, someone was detected at %1.").arg(localUnknownTime(snapshot.unknownOccupant));
        } else {
            result.response = QStringLiteral("No unknown room presence was recorded while you were away.");
        }
    } else {
        result.response = summarizeRoomStatus(snapshot, intent);
    }
    return result;
}

QString LocalSmartHomeIntentRouter::summarizeRoomStatus(const SmartHomeSnapshot &snapshot, const LocalSmartHomeIntent &) const
{
    if (snapshot.roomState == SmartRoomOccupancyState::UNKNOWN_OCCUPANT_IN_ROOM || snapshot.unknownOccupant.active) {
        return QStringLiteral("Yes, someone appears to be in the room, but your phone is not detected as home.");
    }
    if (snapshot.roomState == SmartRoomOccupancyState::IN_ROOM) {
        return QStringLiteral("Yes, your phone is home and the room sensor is occupied.");
    }
    if (snapshot.roomState == SmartRoomOccupancyState::HOME_NOT_ROOM) {
        return QStringLiteral("Your phone is home, but the room sensor is not occupied.");
    }
    if (snapshot.roomState == SmartRoomOccupancyState::AWAY) {
        return QStringLiteral("No, your phone is not detected as home.");
    }
    if (snapshot.presence.available && snapshot.presence.occupied) {
        return QStringLiteral("Yes, the room sensor is occupied. %1.").arg(identityText(snapshot));
    }
    if (snapshot.presence.available) {
        return QStringLiteral("No, the room sensor is clear. %1.").arg(identityText(snapshot));
    }
    return QStringLiteral("Room occupancy is unknown. %1.").arg(identityText(snapshot));
}

QString LocalSmartHomeIntentRouter::summarizeLightStatus(const SmartHomeSnapshot &snapshot) const
{
    if (!snapshot.light.available) {
        return QStringLiteral("Smart light status is unavailable.");
    }
    if (!snapshot.light.on) {
        return QStringLiteral("The light is off.");
    }
    if (snapshot.light.brightnessPercent >= 0) {
        return QStringLiteral("The light is on at %1%.").arg(snapshot.light.brightnessPercent);
    }
    return QStringLiteral("The light is on.");
}

void LocalSmartHomeIntentRouter::logRoute(const LocalSmartHomeRouteResult &result) const
{
    if (!m_loggingService) {
        return;
    }
    m_loggingService->infoFor(
        QStringLiteral("tools_mcp"),
        QStringLiteral("[smart_home.local_route] action=%1 success=%2 latencyMs=%3")
            .arg(result.action,
                 result.success ? QStringLiteral("true") : QStringLiteral("false"),
                 QString::number(result.latencyMs)));
    m_loggingService->infoFor(
        QStringLiteral("tools_mcp"),
        QStringLiteral("[smart_home.backend_route_bypassed] action=%1 reason=deterministic_smart_home")
            .arg(result.action));
}
