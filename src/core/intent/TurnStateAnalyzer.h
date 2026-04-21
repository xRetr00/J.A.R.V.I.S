#pragma once

#include "core/intent/SmartIntentTypes.h"

class TurnStateAnalyzer
{
public:
    TurnState analyze(const TurnStateInput &input) const;
};
