#pragma once

#include <QString>

#include "core/intent/SmartIntentTypes.h"

struct IntentTuningThresholds
{
    float highAmbiguity = 0.6f;
    float lowConfidence = 0.42f;
    float mediumConfidence = 0.66f;
    float highConfidence = 0.78f;
    float backendAssistNeed = 0.55f;
};

class IntentTuningConfig
{
public:
    static const IntentTuningThresholds &thresholds();
    static IntentAdvisorMode advisorModeFromEnvironment();
    static QString advisorModeToString(IntentAdvisorMode mode);
};

