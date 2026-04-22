#pragma once

#include "core/intent/SmartIntentTypes.h"

class LocalIntentAdvisor
{
public:
    IntentAdvisorSuggestion suggest(const TurnSignals &turnSignals,
                                    const TurnGoalSet &goals,
                                    const TurnState &turnState,
                                    const QList<ExecutionIntentCandidate> &candidates,
                                    IntentAdvisorMode mode = IntentAdvisorMode::Heuristic) const;
};
