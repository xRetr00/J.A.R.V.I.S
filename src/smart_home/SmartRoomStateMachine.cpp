#include "smart_home/SmartRoomStateMachine.h"

#include <algorithm>

namespace {
constexpr qint64 kMinuteMs = 60 * 1000;

qint64 positiveElapsed(qint64 sinceMs, qint64 nowMs)
{
    if (sinceMs <= 0 || nowMs <= sinceMs) {
        return 0;
    }
    return nowMs - sinceMs;
}
}

QString smartRoomOccupancyStateName(SmartRoomOccupancyState state)
{
    switch (state) {
    case SmartRoomOccupancyState::AWAY:
        return QStringLiteral("AWAY");
    case SmartRoomOccupancyState::HOME_NOT_ROOM:
        return QStringLiteral("HOME_NOT_ROOM");
    case SmartRoomOccupancyState::IN_ROOM:
        return QStringLiteral("IN_ROOM");
    case SmartRoomOccupancyState::ROOM_OCCUPIED_SENSOR_ONLY:
        return QStringLiteral("ROOM_OCCUPIED_SENSOR_ONLY");
    case SmartRoomOccupancyState::UNKNOWN:
    default:
        return QStringLiteral("UNKNOWN");
    }
}

SmartRoomTransition SmartRoomStateMachine::evaluate(const SmartRoomStateMachineInput &input)
{
    const qint64 nowMs = input.nowMs > 0 ? input.nowMs : QDateTime::currentMSecsSinceEpoch();
    const SmartRoomOccupancyState previous = m_currentState;
    SmartRoomTransition transition;
    transition.previousState = previous;
    transition.occurredAtMs = nowMs;

    const bool presenceAvailable = input.presence.has_value() && input.presence->available && !input.presence->stale;
    const bool roomOccupied = presenceAvailable && input.presence->occupied;
    transition.roomOccupied = roomOccupied;
    if (roomOccupied) {
        m_lastOccupiedAtMs = nowMs;
    }

    const bool identityAvailable = input.identity.has_value() && input.identity->available && !input.identity->stale;
    if (identityAvailable && input.identity->present) {
        m_lastIdentityPresentAtMs = nowMs;
        transition.phonePresent = true;
    }

    const int awayTimeoutMinutes = std::max(1, input.config.bleAwayTimeoutMinutes);
    const int absenceGraceMinutes = std::clamp(input.config.roomAbsenceGraceMinutes, 0, 30);

    if (identityAvailable) {
        if (input.identity->present) {
            m_currentState = roomOccupied
                ? SmartRoomOccupancyState::IN_ROOM
                : SmartRoomOccupancyState::HOME_NOT_ROOM;
            transition.reasonCode = roomOccupied
                ? QStringLiteral("smart_room.identity_and_sensor_present")
                : QStringLiteral("smart_room.identity_present_sensor_clear");
        } else {
            const qint64 missingFromMs = input.identity->observedAtMs > 0
                ? input.identity->observedAtMs
                : m_lastIdentityPresentAtMs;
            transition.bleMissingMs = positiveElapsed(missingFromMs, nowMs);
            if (transition.bleMissingMs >= awayTimeoutMinutes * kMinuteMs) {
                m_currentState = SmartRoomOccupancyState::AWAY;
                transition.reasonCode = QStringLiteral("smart_room.identity_absent_timeout");
            } else if (roomOccupied) {
                m_currentState = SmartRoomOccupancyState::ROOM_OCCUPIED_SENSOR_ONLY;
                transition.reasonCode = QStringLiteral("smart_room.sensor_occupied_identity_missing_grace");
            } else {
                m_currentState = SmartRoomOccupancyState::UNKNOWN;
                transition.reasonCode = QStringLiteral("smart_room.identity_missing_pending_timeout");
            }
        }
    } else if (presenceAvailable) {
        if (roomOccupied) {
            m_currentState = SmartRoomOccupancyState::ROOM_OCCUPIED_SENSOR_ONLY;
            transition.reasonCode = QStringLiteral("smart_room.sensor_only_occupied");
        } else {
            transition.sensorOffGapMs = positiveElapsed(m_lastOccupiedAtMs, nowMs);
            if (previous == SmartRoomOccupancyState::ROOM_OCCUPIED_SENSOR_ONLY
                && absenceGraceMinutes > 0
                && transition.sensorOffGapMs < absenceGraceMinutes * kMinuteMs) {
                m_currentState = SmartRoomOccupancyState::ROOM_OCCUPIED_SENSOR_ONLY;
                transition.reasonCode = QStringLiteral("smart_room.sensor_absence_grace");
            } else {
                m_currentState = SmartRoomOccupancyState::UNKNOWN;
                transition.reasonCode = QStringLiteral("smart_room.sensor_clear_no_identity");
            }
        }
    } else {
        m_currentState = SmartRoomOccupancyState::UNKNOWN;
        transition.reasonCode = QStringLiteral("smart_room.no_available_inputs");
    }

    transition.currentState = m_currentState;
    return transition;
}

SmartRoomOccupancyState SmartRoomStateMachine::currentState() const
{
    return m_currentState;
}
