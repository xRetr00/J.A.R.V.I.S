#pragma once

#include "smart_home/SmartHomeTypes.h"

class SmartRoomStateMachine
{
public:
    SmartRoomTransition evaluate(const SmartRoomStateMachineInput &input);
    SmartRoomOccupancyState currentState() const;

private:
    SmartRoomOccupancyState m_currentState = SmartRoomOccupancyState::UNKNOWN;
    qint64 m_lastOccupiedAtMs = 0;
    qint64 m_lastIdentityPresentAtMs = 0;
    bool m_returnedHomeFromAway = false;
};
