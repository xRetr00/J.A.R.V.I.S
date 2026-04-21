#pragma once

#include "core/intent/SmartIntentTypes.h"

class TurnSignalExtractor
{
public:
    TurnSignals extract(const QString &input) const;
};
