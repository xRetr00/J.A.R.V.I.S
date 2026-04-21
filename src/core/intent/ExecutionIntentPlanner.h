#pragma once

#include "core/intent/SmartIntentTypes.h"

class ExecutionIntentPlanner
{
public:
    QList<ExecutionIntentCandidate> plan(const TurnGoalSet &goals,
                                         const TurnSignals &turnSignals,
                                         bool hasDeterministicTask,
                                         const AgentTask &deterministicTask) const;
};
