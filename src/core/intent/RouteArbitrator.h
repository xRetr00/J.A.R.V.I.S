#pragma once

#include "core/intent/SmartIntentTypes.h"

class RouteArbitrator
{
public:
    RouteArbitrationResult arbitrate(const InputRouteDecision &policyDecision,
                                     const TurnSignals &turnSignals,
                                     const TurnState &state,
                                     const TurnGoalSet &goals,
                                     const QList<ExecutionIntentCandidate> &candidates,
                                     bool hasDeterministicTask) const;
};
