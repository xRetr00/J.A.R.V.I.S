#include "core/intent/ExecutionIntentPlanner.h"

#include <algorithm>

namespace {
ExecutionIntentCandidate makeCandidate(ExecutionIntentKind kind,
                                       InputRouteKind routeKind,
                                       float score,
                                       const QString &reason,
                                       int backendPriority = -1,
                                       float confidencePenalty = 0.0f)
{
    ExecutionIntentCandidate candidate;
    candidate.kind = kind;
    candidate.route.kind = routeKind;
    candidate.score = score;
    candidate.requiresBackend = (routeKind == InputRouteKind::Conversation
                                 || routeKind == InputRouteKind::AgentConversation
                                 || routeKind == InputRouteKind::CommandExtraction);
    candidate.canRunLocal = (routeKind == InputRouteKind::LocalResponse
                             || routeKind == InputRouteKind::DeterministicTasks
                             || routeKind == InputRouteKind::BackgroundTasks
                             || routeKind == InputRouteKind::AgentCapabilityError);
    candidate.backendPriority = backendPriority >= 0
        ? backendPriority
        : (candidate.requiresBackend ? 70 : 10);
    candidate.confidencePenalty = confidencePenalty;
    if (!reason.isEmpty()) {
        candidate.reasonCodes.push_back(reason);
    }
    return candidate;
}
}

QList<ExecutionIntentCandidate> ExecutionIntentPlanner::plan(const TurnGoalSet &goals,
                                                             const TurnSignals &turnSignals,
                                                             bool hasDeterministicTask,
                                                             const AgentTask &deterministicTask) const
{
    QList<ExecutionIntentCandidate> candidates;

    if (goals.primaryGoal.kind == UserGoalKind::Social) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::LocalResponse,
            InputRouteKind::LocalResponse,
            0.92f,
            QStringLiteral("planner.social_local_response"),
            5,
            0.02f));
    }

    if (goals.primaryGoal.kind == UserGoalKind::DeterministicAction
        || hasDeterministicTask
        || (turnSignals.hasGreeting && hasDeterministicTask)) {
        ExecutionIntentCandidate deterministic = makeCandidate(
            ExecutionIntentKind::DeterministicTask,
            InputRouteKind::DeterministicTasks,
            0.95f,
            QStringLiteral("planner.deterministic_task"),
            0,
            0.01f);
        if (hasDeterministicTask && !deterministicTask.type.trimmed().isEmpty()) {
            deterministic.tasks = {deterministicTask};
        }
        candidates.push_back(deterministic);
    }

    if (goals.primaryGoal.kind == UserGoalKind::Continuation) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::AgentConversation,
            InputRouteKind::AgentConversation,
            0.78f,
            QStringLiteral("planner.continuation_agent"),
            85,
            0.08f));
    }

    if (goals.primaryGoal.kind == UserGoalKind::CommandRequest) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::CommandExtraction,
            InputRouteKind::CommandExtraction,
            0.8f,
            QStringLiteral("planner.command_extraction"),
            65,
            0.1f));
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::AgentConversation,
            InputRouteKind::AgentConversation,
            0.74f,
            QStringLiteral("planner.command_agent_backend"),
            84,
            0.07f));
    }

    if (goals.primaryGoal.kind == UserGoalKind::InfoQuery) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::BackendReasoning,
            InputRouteKind::Conversation,
            0.82f,
            QStringLiteral("planner.info_query_conversation"),
            80,
            0.04f));
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::AgentConversation,
            InputRouteKind::AgentConversation,
            0.79f,
            QStringLiteral("planner.info_query_agent_backend"),
            88,
            0.05f));
    }

    if (goals.primaryGoal.kind == UserGoalKind::ContextReference) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::BackgroundTask,
            InputRouteKind::BackgroundTasks,
            0.65f,
            QStringLiteral("planner.context_reference_background"),
            20,
            0.06f));
    }

    if (goals.ambiguity >= 0.5f) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::AskClarification,
            InputRouteKind::LocalResponse,
            0.9f,
            QStringLiteral("planner.ask_clarification_high_ambiguity"),
            0,
            0.0f));
    }

    if (goals.primaryGoal.kind == UserGoalKind::Confirmation) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::AskClarification,
            InputRouteKind::LocalResponse,
            0.7f,
            QStringLiteral("planner.confirmation_local_response"),
            0,
            0.02f));
    }

    if (candidates.isEmpty()) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::BackendReasoning,
            InputRouteKind::Conversation,
            0.5f,
            QStringLiteral("planner.default_conversation"),
            75,
            0.08f));
    }

    if (goals.mixedIntent || goals.ambiguity >= 0.5f) {
        const float ambiguityPenalty = std::clamp(goals.ambiguity * 0.2f, 0.0f, 0.2f);
        for (ExecutionIntentCandidate &candidate : candidates) {
            candidate.confidencePenalty = std::clamp(candidate.confidencePenalty + ambiguityPenalty, 0.0f, 0.4f);
            if (candidate.requiresBackend) {
                candidate.backendPriority = std::max(candidate.backendPriority, 85);
                candidate.reasonCodes.push_back(QStringLiteral("planner.backend_priority_ambiguity"));
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ExecutionIntentCandidate &left, const ExecutionIntentCandidate &right) {
        if (left.score == right.score) {
            return left.backendPriority > right.backendPriority;
        }
        return left.score > right.score;
    });

    return candidates;
}
