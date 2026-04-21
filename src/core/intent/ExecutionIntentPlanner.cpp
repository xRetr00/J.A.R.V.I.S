#include "core/intent/ExecutionIntentPlanner.h"

#include <algorithm>

namespace {
ExecutionIntentCandidate makeCandidate(ExecutionIntentKind kind,
                                       InputRouteKind routeKind,
                                       float score,
                                       const QString &reason)
{
    ExecutionIntentCandidate candidate;
    candidate.kind = kind;
    candidate.route.kind = routeKind;
    candidate.score = score;
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
            QStringLiteral("planner.social_local_response")));
    }

    if (goals.primaryGoal.kind == UserGoalKind::DeterministicAction
        || hasDeterministicTask
        || (turnSignals.hasGreeting && hasDeterministicTask)) {
        ExecutionIntentCandidate deterministic = makeCandidate(
            ExecutionIntentKind::DeterministicTask,
            InputRouteKind::DeterministicTasks,
            0.95f,
            QStringLiteral("planner.deterministic_task"));
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
            QStringLiteral("planner.continuation_agent")));
    }

    if (goals.primaryGoal.kind == UserGoalKind::CommandRequest) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::CommandExtraction,
            InputRouteKind::CommandExtraction,
            0.8f,
            QStringLiteral("planner.command_extraction")));
    }

    if (goals.primaryGoal.kind == UserGoalKind::InfoQuery) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::BackendReasoning,
            InputRouteKind::Conversation,
            0.82f,
            QStringLiteral("planner.info_query_conversation")));
    }

    if (goals.primaryGoal.kind == UserGoalKind::ContextReference) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::BackgroundTask,
            InputRouteKind::BackgroundTasks,
            0.65f,
            QStringLiteral("planner.context_reference_background")));
    }

    if (goals.primaryGoal.kind == UserGoalKind::Confirmation) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::AskClarification,
            InputRouteKind::LocalResponse,
            0.7f,
            QStringLiteral("planner.confirmation_local_response")));
    }

    if (candidates.isEmpty()) {
        candidates.push_back(makeCandidate(
            ExecutionIntentKind::BackendReasoning,
            InputRouteKind::Conversation,
            0.5f,
            QStringLiteral("planner.default_conversation")));
    }

    std::sort(candidates.begin(), candidates.end(), [](const ExecutionIntentCandidate &left, const ExecutionIntentCandidate &right) {
        return left.score > right.score;
    });

    return candidates;
}
