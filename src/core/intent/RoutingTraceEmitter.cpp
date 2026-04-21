#include "core/intent/RoutingTraceEmitter.h"

#include <QJsonArray>
#include <QJsonDocument>

#include "logging/LoggingService.h"

namespace {
QString inputRouteKindToString(InputRouteKind kind)
{
    switch (kind) {
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

QJsonObject routeDecisionToJson(const InputRouteDecision &decision)
{
    QJsonObject object;
    object.insert(QStringLiteral("kind"), inputRouteKindToString(decision.kind));
    object.insert(QStringLiteral("intent"), static_cast<int>(decision.intent));
    object.insert(QStringLiteral("task_count"), decision.tasks.size());
    object.insert(QStringLiteral("status"), decision.status);
    return object;
}

QJsonArray stringsToArray(const QStringList &items)
{
    QJsonArray array;
    for (const QString &item : items) {
        array.push_back(item);
    }
    return array;
}
}

QJsonObject RoutingTraceEmitter::buildRouteFinalPayload(const RoutingTrace &trace) const
{
    QJsonObject payload;
    payload.insert(QStringLiteral("record"), QStringLiteral("route_final"));
    payload.insert(QStringLiteral("raw_input"), trace.rawInput);
    payload.insert(QStringLiteral("normalized_input"), trace.normalizedInput);
    payload.insert(QStringLiteral("deterministic_matched"), trace.deterministicMatched);
    payload.insert(QStringLiteral("deterministic_task_type"), trace.deterministicTaskType);
    payload.insert(QStringLiteral("used_arbitrator_authority"), trace.usedArbitratorAuthority);
    payload.insert(QStringLiteral("final_executed_route"), trace.finalExecutedRoute);
    payload.insert(QStringLiteral("confirmation_gate_triggered"), trace.confirmationGateTriggered);
    payload.insert(QStringLiteral("confirmation_outcome"), trace.confirmationOutcome);
    payload.insert(QStringLiteral("reason_codes"), stringsToArray(trace.reasonCodes));
    payload.insert(QStringLiteral("overrides"), stringsToArray(trace.overridesApplied));

    QJsonObject signalObject;
    signalObject.insert(QStringLiteral("has_greeting"), trace.turnSignals.hasGreeting);
    signalObject.insert(QStringLiteral("has_smalltalk"), trace.turnSignals.hasSmallTalk);
    signalObject.insert(QStringLiteral("social_only"), trace.turnSignals.socialOnly);
    signalObject.insert(QStringLiteral("has_command_cue"), trace.turnSignals.hasCommandCue);
    signalObject.insert(QStringLiteral("has_question_cue"), trace.turnSignals.hasQuestionCue);
    signalObject.insert(QStringLiteral("has_context_reference"), trace.turnSignals.hasContextReference);
    signalObject.insert(QStringLiteral("has_continuation_cue"), trace.turnSignals.hasContinuationCue);
    signalObject.insert(QStringLiteral("matched_cues"), stringsToArray(trace.turnSignals.matchedCues));
    payload.insert(QStringLiteral("turn_signals"), signalObject);

    QJsonObject legacyObject;
    legacyObject.insert(QStringLiteral("local_intent"), static_cast<int>(trace.legacySignals.localIntent));
    legacyObject.insert(QStringLiteral("likely_command"), trace.legacySignals.likelyCommand);
    legacyObject.insert(QStringLiteral("has_deterministic_task"), trace.legacySignals.hasDeterministicTask);
    legacyObject.insert(QStringLiteral("explicit_web_search"), trace.legacySignals.explicitWebSearch);
    legacyObject.insert(QStringLiteral("likely_knowledge_lookup"), trace.legacySignals.likelyKnowledgeLookup);
    legacyObject.insert(QStringLiteral("freshness_sensitive"), trace.legacySignals.freshnessSensitive);
    legacyObject.insert(QStringLiteral("desktop_context_recall"), trace.legacySignals.desktopContextRecall);
    legacyObject.insert(QStringLiteral("explicit_agent_world_query"), trace.legacySignals.explicitAgentWorldQuery);
    legacyObject.insert(QStringLiteral("explicit_computer_control"), trace.legacySignals.explicitComputerControl);
    payload.insert(QStringLiteral("legacy_signals"), legacyObject);

    QJsonObject turnState;
    turnState.insert(QStringLiteral("is_new_turn"), trace.turnState.isNewTurn);
    turnState.insert(QStringLiteral("is_continuation"), trace.turnState.isContinuation);
    turnState.insert(QStringLiteral("is_confirmation_reply"), trace.turnState.isConfirmationReply);
    turnState.insert(QStringLiteral("is_correction"), trace.turnState.isCorrection);
    turnState.insert(QStringLiteral("refers_to_previous_task"), trace.turnState.refersToPreviousTask);
    turnState.insert(QStringLiteral("reason_codes"), stringsToArray(trace.turnState.reasonCodes));
    payload.insert(QStringLiteral("turn_state"), turnState);

    QJsonObject goals;
    goals.insert(QStringLiteral("primary_kind"), static_cast<int>(trace.goals.primaryGoal.kind));
    goals.insert(QStringLiteral("primary_label"), trace.goals.primaryGoal.label);
    goals.insert(QStringLiteral("primary_confidence"), trace.goals.primaryGoal.confidence);
    goals.insert(QStringLiteral("mixed_intent"), trace.goals.mixedIntent);
    goals.insert(QStringLiteral("ambiguity"), trace.goals.ambiguity);
    if (trace.goals.secondaryGoal.has_value()) {
        goals.insert(QStringLiteral("secondary_kind"), static_cast<int>(trace.goals.secondaryGoal->kind));
        goals.insert(QStringLiteral("secondary_label"), trace.goals.secondaryGoal->label);
    }
    payload.insert(QStringLiteral("goals"), goals);

    QJsonArray candidateArray;
    for (const ExecutionIntentCandidate &candidate : trace.candidates) {
        QJsonObject candidateObject;
        candidateObject.insert(QStringLiteral("kind"), static_cast<int>(candidate.kind));
        candidateObject.insert(QStringLiteral("route"), inputRouteKindToString(candidate.route.kind));
        candidateObject.insert(QStringLiteral("score"), candidate.score);
        candidateObject.insert(QStringLiteral("reason_codes"), stringsToArray(candidate.reasonCodes));
        candidateArray.push_back(candidateObject);
    }
    payload.insert(QStringLiteral("execution_candidates"), candidateArray);

    QJsonObject intentSnapshot;
    intentSnapshot.insert(QStringLiteral("ml_intent"), static_cast<int>(trace.intentSnapshot.mlIntent));
    intentSnapshot.insert(QStringLiteral("ml_confidence"), trace.intentSnapshot.mlConfidence);
    intentSnapshot.insert(QStringLiteral("detector_intent"), static_cast<int>(trace.intentSnapshot.detectorIntent));
    intentSnapshot.insert(QStringLiteral("detector_confidence"), trace.intentSnapshot.detectorConfidence);
    intentSnapshot.insert(QStringLiteral("effective_intent"), static_cast<int>(trace.intentSnapshot.effectiveIntent));
    intentSnapshot.insert(QStringLiteral("effective_confidence"), trace.intentSnapshot.effectiveConfidence);
    payload.insert(QStringLiteral("intent_snapshot"), intentSnapshot);

    payload.insert(QStringLiteral("policy_decision"), routeDecisionToJson(trace.policyDecision));
    payload.insert(QStringLiteral("arbitrator_decision"), routeDecisionToJson(trace.arbitratorResult.decision));
    payload.insert(QStringLiteral("final_decision"), routeDecisionToJson(trace.finalDecision));
    payload.insert(QStringLiteral("arbitrator_reason_codes"), stringsToArray(trace.arbitratorResult.reasonCodes));
    payload.insert(QStringLiteral("arbitrator_overrides"), stringsToArray(trace.arbitratorResult.overridesApplied));

    return payload;
}

void RoutingTraceEmitter::emitRouteFinal(LoggingService *loggingService, const RoutingTrace &trace) const
{
    if (loggingService == nullptr) {
        return;
    }
    const QJsonDocument document(buildRouteFinalPayload(trace));
    loggingService->infoFor(
        QStringLiteral("route_audit"),
        QStringLiteral("[route_final] %1").arg(QString::fromUtf8(document.toJson(QJsonDocument::Compact))));
}
