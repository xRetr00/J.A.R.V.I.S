#pragma once

#include "core/intent/SmartIntentTypes.h"

class IntentConfidenceCalculator
{
public:
    IntentConfidence compute(const TurnSignals &turnSignals,
                             const TurnGoalSet &goals,
                             const QList<ExecutionIntentCandidate> &candidates) const;
    float computeAmbiguity(const TurnSignals &turnSignals,
                           const TurnGoalSet &goals,
                           const QList<ExecutionIntentCandidate> &candidates,
                           const IntentConfidence &confidence) const;
};
