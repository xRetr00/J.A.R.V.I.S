#pragma once

#include <optional>

#include <QDateTime>
#include <QJsonObject>
#include <QString>

enum class SmartRoomOccupancyState {
    UNKNOWN,
    AWAY,
    HOME_NOT_ROOM,
    IN_ROOM,
    ROOM_OCCUPIED_SENSOR_ONLY
};

struct SmartHomeConfig
{
    bool enabled = false;
    QString provider = QStringLiteral("home_assistant");
    QString homeAssistantBaseUrl;
    QString homeAssistantTokenEnvVar = QStringLiteral("VAXIL_HOME_ASSISTANT_TOKEN");
    QString presenceEntityId;
    QString lightEntityId;
    int pollIntervalMs = 5000;
    bool sensorOnlyWelcomeEnabled = false;
    int welcomeCooldownMinutes = 30;
    int roomAbsenceGraceMinutes = 6;
    int requestTimeoutMs = 5000;
    int bleAwayTimeoutMinutes = 10;
};

struct SmartRoomStateMachineConfig
{
    bool sensorOnlyWelcomeEnabled = false;
    int roomAbsenceGraceMinutes = 6;
    int bleAwayTimeoutMinutes = 10;
};

struct SmartPresenceSnapshot
{
    QString roomId = QStringLiteral("default");
    QString sensorId;
    bool available = false;
    bool occupied = false;
    qint64 observedAtMs = 0;
    QString source;
    QString rawState;
    bool stale = false;
};

struct SmartLightSnapshot
{
    QString lightId;
    bool available = false;
    bool on = false;
    int brightnessPercent = -1;
    QString colorName;
    QString scene;
    bool colorSupported = false;
    qint64 observedAtMs = 0;
    QString source;
    QString rawState;
    bool stale = false;
};

struct BleIdentitySnapshot
{
    QString identityId;
    bool available = false;
    bool present = false;
    int rssi = 0;
    qint64 observedAtMs = 0;
    QString source;
    bool stale = false;
};

struct SmartHomeSnapshot
{
    SmartPresenceSnapshot presence;
    SmartLightSnapshot light;
    bool success = false;
    QString summary;
    QString detail;
    QString errorKind;
    int httpStatus = 0;
    qint64 latencyMs = 0;
};

struct SmartRoomStateMachineInput
{
    std::optional<SmartPresenceSnapshot> presence;
    std::optional<BleIdentitySnapshot> identity;
    SmartRoomStateMachineConfig config;
    qint64 nowMs = 0;
};

struct SmartRoomTransition
{
    SmartRoomOccupancyState previousState = SmartRoomOccupancyState::UNKNOWN;
    SmartRoomOccupancyState currentState = SmartRoomOccupancyState::UNKNOWN;
    QString reasonCode;
    bool phonePresent = false;
    bool roomOccupied = false;
    qint64 bleMissingMs = -1;
    qint64 sensorOffGapMs = -1;
    qint64 occurredAtMs = 0;
};

struct SmartWelcomeDecision
{
    bool allowed = false;
    QString reasonCode;
    QString message;
    qint64 nextLastWelcomeAtMs = 0;
};

struct SmartLightCommand
{
    QString action;
    int brightnessPercent = -1;
    QString colorName;
};

struct SmartLightCommandResult
{
    bool success = false;
    QString summary;
    QString detail;
    QString errorKind;
    int httpStatus = 0;
    qint64 latencyMs = 0;
};

QString smartRoomOccupancyStateName(SmartRoomOccupancyState state);
