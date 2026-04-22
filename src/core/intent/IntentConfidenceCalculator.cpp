#include "core/intent/IntentConfidenceCalculator.h"

#include <algorithm>

namespace {
float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}
}

IntentConfidence IntentConfidenceCalculator::compute(const TurnSignals &turnSignals,
                                                     const TurnGoalSet &goals,
                                                     const QList<ExecutionIntentCandidate> &candidates) const
{
    IntentConfidence confidence;

    float signal = 0.2f;
    if (turnSignals.socialOnly || turnSignals.hasDeterministicCue || turnSignals.hasContinuationCue) {
        signal += 0.45f;
    }
    if (turnSignals.hasCommandCue || turnSignals.hasQuestionCue || turnSignals.hasContextReference) {
        signal += 0.2f;
    }
    if (turnSignals.hasCommandCue && turnSignals.hasQuestionCue) {
        signal -= 0.15f;
    }
    confidence.signalConfidence = clamp01(signal);

    float goal = goals.primaryGoal.confidence;
    if (goals.mixedIntent) {
        goal -= 0.12f;
    }
    goal -= (goals.ambiguity * 0.25f);
    confidence.goalConfidence = clamp01(goal);

    float execution = 0.3f;
    if (!candidates.isEmpty()) {
        const float top = candidates.first().score - candidates.first().confidencePenalty;
        float gap = top;
        if (candidates.size() > 1) {
            const float second = candidates.at(1).score - candidates.at(1).confidencePenalty;
            gap = std::max(0.0f, top - second);
        }
        execution = clamp01((top * 0.75f) + (gap * 0.5f));
    }
    confidence.executionConfidence = execution;
    confidence.finalConfidence = clamp01((confidence.signalConfidence * 0.25f)
                                         + (confidence.goalConfidence * 0.35f)
                                         + (confidence.executionConfidence * 0.4f));
    return confidence;
}

float IntentConfidenceCalculator::computeAmbiguity(const TurnSignals &turnSignals,
                                                   const TurnGoalSet &goals,
                                                   const QList<ExecutionIntentCandidate> &candidates,
                                                   const IntentConfidence &confidence) const
{
    float ambiguity = goals.ambiguity;
    if (turnSignals.hasCommandCue && turnSignals.hasQuestionCue) {
        ambiguity += 0.2f;
    }
    if (candidates.size() > 1) {
        const float top = candidates.first().score - candidates.first().confidencePenalty;
        const float second = candidates.at(1).score - candidates.at(1).confidencePenalty;
        const float gap = std::max(0.0f, top - second);
        ambiguity += std::max(0.0f, 0.3f - gap);
    }
    if (confidence.finalConfidence < 0.5f) {
        ambiguity += 0.2f;
    }
    return clamp01(ambiguity);
}
