#pragma once

#include "core/intent/SmartIntentTypes.h"

class UserGoalInferer
{
public:
    TurnGoalSet infer(const TurnSignals &turnSignals,
                      const TurnState &state,
                      bool hasDeterministicTask) const;
};
