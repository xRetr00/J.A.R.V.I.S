#include "core/intent/IntentTuningConfig.h"

#include <QByteArray>

const IntentTuningThresholds &IntentTuningConfig::thresholds()
{
    static const IntentTuningThresholds kThresholds;
    return kThresholds;
}

IntentAdvisorMode IntentTuningConfig::advisorModeFromEnvironment()
{
    const QByteArray value = qgetenv("VAXIL_INTENT_ADVISOR_MODE").trimmed().toLower();
    if (value == "learned") {
        return IntentAdvisorMode::Learned;
    }
    if (value == "shadow" || value == "shadow_learned") {
        return IntentAdvisorMode::ShadowLearned;
    }
    return IntentAdvisorMode::Heuristic;
}

QString IntentTuningConfig::advisorModeToString(IntentAdvisorMode mode)
{
    switch (mode) {
    case IntentAdvisorMode::Heuristic:
        return QStringLiteral("heuristic");
    case IntentAdvisorMode::ShadowLearned:
        return QStringLiteral("shadow_learned");
    case IntentAdvisorMode::Learned:
        return QStringLiteral("learned");
    }
    return QStringLiteral("heuristic");
}

