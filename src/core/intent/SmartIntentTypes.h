#pragma once

#include <optional>

#include <QList>
#include <QString>
#include <QStringList>

#include "core/AssistantTypes.h"

enum class TurnSignalOwner {
    TurnSignalExtractor,
    TurnStateAnalyzer,
    UserGoalInferer,
    ExecutionIntentPlanner,
    RouteArbitrator
};

enum class UserGoalKind {
    Social,
    InfoQuery,
    CommandRequest,
    DeterministicAction,
    ContextReference,
    Continuation,
    Confirmation,
    Correction,
    MemoryUpdate,
    ToolInventory,
    Unknown
};

enum class ExecutionIntentKind {
    LocalResponse,
    DeterministicTask,
    BackgroundTask,
    CommandExtraction,
    AgentConversation,
    BackendReasoning,
    AskClarification,
    CapabilityError
};

struct TurnSignals {
    QString rawInput;
    QString normalizedInput;
    bool hasGreeting = false;
    bool hasSmallTalk = false;
    bool socialOnly = false;
    bool hasCommandCue = false;
    bool hasQuestionCue = false;
    bool hasNegation = false;
    bool hasUrgency = false;
    bool hasDeterministicCue = false;
    bool hasContextReference = false;
    bool hasContinuationCue = false;
    QStringList matchedCues;
};

struct TurnState {
    bool isNewTurn = true;
    bool isContinuation = false;
    bool isConfirmationReply = false;
    bool isCorrection = false;
    bool refersToPreviousTask = false;
    QStringList reasonCodes;
};

struct TurnStateInput {
    QString normalizedInput;
    TurnSignals turnSignals;
    bool hasPendingConfirmation = false;
    bool hasUsableActionThread = false;
    bool hasAnyActionThread = false;
};

struct UserGoal {
    UserGoalKind kind = UserGoalKind::Unknown;
    QString label;
    float confidence = 0.0f;
    QStringList reasonCodes;
};

struct TurnGoalSet {
    UserGoal primaryGoal;
    std::optional<UserGoal> secondaryGoal;
    bool mixedIntent = false;
    float ambiguity = 0.0f;
    float confidence = 0.0f;
};

struct ExecutionIntentCandidate {
    ExecutionIntentKind kind = ExecutionIntentKind::BackendReasoning;
    InputRouteDecision route;
    QList<AgentTask> tasks;
    float score = 0.0f;
    QStringList reasonCodes;
};

struct RouteScores {
    float socialScore = 0.0f;
    float commandScore = 0.0f;
    float infoQueryScore = 0.0f;
    float deterministicScore = 0.0f;
    float continuationScore = 0.0f;
    float contextReferenceScore = 0.0f;
    float backendNeedScore = 0.0f;
};

struct RouteArbitrationResult {
    InputRouteDecision decision;
    RouteScores scores;
    QString finalRoute;
    float confidence = 0.0f;
    QStringList reasonCodes;
    QStringList overridesApplied;
};

struct LegacyRoutingSignals {
    LocalIntent localIntent = LocalIntent::Unknown;
    bool likelyCommand = false;
    bool hasDeterministicTask = false;
    bool explicitWebSearch = false;
    bool likelyKnowledgeLookup = false;
    bool freshnessSensitive = false;
    bool desktopContextRecall = false;
    bool explicitAgentWorldQuery = false;
    bool explicitComputerControl = false;
};

struct IntentInferenceSnapshot {
    IntentType mlIntent = IntentType::GENERAL_CHAT;
    float mlConfidence = 0.0f;
    IntentType detectorIntent = IntentType::GENERAL_CHAT;
    float detectorConfidence = 0.0f;
    IntentType effectiveIntent = IntentType::GENERAL_CHAT;
    float effectiveConfidence = 0.0f;
};

struct RoutingTrace {
    QString rawInput;
    QString normalizedInput;
    TurnSignals turnSignals;
    LegacyRoutingSignals legacySignals;
    TurnState turnState;
    TurnGoalSet goals;
    QList<ExecutionIntentCandidate> candidates;
    IntentInferenceSnapshot intentSnapshot;
    bool deterministicMatched = false;
    QString deterministicTaskType;
    InputRouteDecision policyDecision;
    RouteArbitrationResult arbitratorResult;
    InputRouteDecision finalDecision;
    bool usedArbitratorAuthority = false;
    bool confirmationGateTriggered = false;
    QString confirmationOutcome;
    QString finalExecutedRoute;
    QStringList overridesApplied;
    QStringList reasonCodes;
};
