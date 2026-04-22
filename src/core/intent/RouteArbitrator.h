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
                                     const IntentConfidence &confidence,
                                     float ambiguityScore,
                                     const IntentAdvisorSuggestion &advisorSuggestion,
                                     bool hasDeterministicTask) const;
};
