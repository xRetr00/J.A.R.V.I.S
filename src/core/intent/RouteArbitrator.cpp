#include "core/intent/RouteArbitrator.h"

namespace {
QString routeToString(const InputRouteDecision &decision)
{
    switch (decision.kind) {
    case InputRouteKind::LocalResponse:
        return QStringLiteral("LocalResponse");
    case InputRouteKind::DeterministicTasks:
        return QStringLiteral("DeterministicTasks");
    case InputRouteKind::BackgroundTasks:
        return QStringLiteral("BackgroundTasks");
    case InputRouteKind::Conversation:
        return QStringLiteral("Conversation");
    case InputRouteKind::AgentConversation:
        return QStringLiteral("AgentConversation");
    case InputRouteKind::CommandExtraction:
        return QStringLiteral("CommandExtraction");
    case InputRouteKind::AgentCapabilityError:
        return QStringLiteral("AgentCapabilityError");
    case InputRouteKind::None:
    default:
        return QStringLiteral("None");
    }
}
}

RouteArbitrationResult RouteArbitrator::arbitrate(const InputRouteDecision &policyDecision,
                                                  const TurnSignals &turnSignals,
                                                  const TurnState &state,
                                                  const TurnGoalSet &goals,
                                                  const QList<ExecutionIntentCandidate> &candidates,
                                                  bool hasDeterministicTask) const
{
    RouteArbitrationResult result;
    result.decision = policyDecision;
    result.finalRoute = routeToString(policyDecision);
    result.confidence = 0.65f;
    result.reasonCodes.push_back(QStringLiteral("arbitrator.shadow_policy_default"));

    result.scores.socialScore = turnSignals.socialOnly ? 0.9f : 0.1f;
    result.scores.commandScore = turnSignals.hasCommandCue ? 0.75f : 0.05f;
    result.scores.infoQueryScore = turnSignals.hasQuestionCue ? 0.8f : 0.1f;
    result.scores.deterministicScore = hasDeterministicTask ? 0.95f : 0.1f;
    result.scores.continuationScore = state.isContinuation ? 0.8f : 0.05f;
    result.scores.contextReferenceScore = turnSignals.hasContextReference ? 0.7f : 0.05f;
    result.scores.backendNeedScore =
        goals.primaryGoal.kind == UserGoalKind::InfoQuery ? 0.8f : 0.2f;

    if (turnSignals.socialOnly) {
        result.decision.kind = InputRouteKind::LocalResponse;
        result.confidence = 0.9f;
        result.reasonCodes = {QStringLiteral("arbitrator.social_only_local_response")};
    } else if (hasDeterministicTask && turnSignals.hasGreeting) {
        result.decision.kind = InputRouteKind::DeterministicTasks;
        result.confidence = 0.95f;
        result.reasonCodes = {QStringLiteral("arbitrator.deterministic_over_social_prefix")};
    } else if (turnSignals.hasCommandCue && turnSignals.hasQuestionCue) {
        result.decision.kind = InputRouteKind::Conversation;
        result.confidence = 0.85f;
        result.reasonCodes = {QStringLiteral("arbitrator.command_vs_info_prefers_info_query")};
    } else if (!candidates.isEmpty()) {
        result.decision = candidates.first().route;
        result.confidence = candidates.first().score;
        result.reasonCodes = candidates.first().reasonCodes;
    }

    result.finalRoute = routeToString(result.decision);
    if (result.decision.kind != policyDecision.kind) {
        result.overridesApplied.push_back(
            QStringLiteral("arbitrator_override:%1->%2")
                .arg(routeToString(policyDecision), routeToString(result.decision)));
    }

    return result;
}
