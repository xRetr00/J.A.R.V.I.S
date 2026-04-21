#include <QtTest>

#include "core/intent/ExecutionIntentPlanner.h"
#include "core/intent/RouteArbitrator.h"
#include "core/intent/RoutingTraceEmitter.h"
#include "core/intent/TurnSignalExtractor.h"
#include "core/intent/TurnStateAnalyzer.h"
#include "core/intent/UserGoalInferer.h"

class SmartIntentV2Tests : public QObject
{
    Q_OBJECT

private slots:
    void analyzesContinuationVsNewTurn();
    void detectsConfirmationReplyState();
    void infersPrimaryAndSecondaryGoals();
    void plansDeterministicCandidateFromSocialPrefixCommand();
    void arbitratesCommandVsInfoConflictToConversation();
    void buildsRouteFinalTracePayload();
};

void SmartIntentV2Tests::analyzesContinuationVsNewTurn()
{
    TurnSignalExtractor extractor;
    TurnStateAnalyzer analyzer;

    TurnStateInput continuationInput;
    continuationInput.normalizedInput = QStringLiteral("open it");
    continuationInput.turnSignals = extractor.extract(continuationInput.normalizedInput);
    continuationInput.hasUsableActionThread = true;
    continuationInput.hasAnyActionThread = true;
    const TurnState continuation = analyzer.analyze(continuationInput);
    QVERIFY(continuation.isContinuation);
    QVERIFY(!continuation.isNewTurn);

    TurnStateInput newTurnInput;
    newTurnInput.normalizedInput = QStringLiteral("search latest amd news");
    newTurnInput.turnSignals = extractor.extract(newTurnInput.normalizedInput);
    newTurnInput.hasUsableActionThread = false;
    const TurnState newTurn = analyzer.analyze(newTurnInput);
    QVERIFY(newTurn.isNewTurn);
}

void SmartIntentV2Tests::detectsConfirmationReplyState()
{
    TurnSignalExtractor extractor;
    TurnStateAnalyzer analyzer;

    TurnStateInput input;
    input.normalizedInput = QStringLiteral("yes, go ahead");
    input.turnSignals = extractor.extract(input.normalizedInput);
    input.hasPendingConfirmation = true;
    const TurnState state = analyzer.analyze(input);
    QVERIFY(state.isConfirmationReply);
}

void SmartIntentV2Tests::infersPrimaryAndSecondaryGoals()
{
    TurnSignalExtractor extractor;
    TurnStateAnalyzer analyzer;
    UserGoalInferer inferer;

    TurnStateInput input;
    input.normalizedInput = QStringLiteral("hi explain this error");
    input.turnSignals = extractor.extract(input.normalizedInput);
    const TurnState state = analyzer.analyze(input);
    const TurnGoalSet goals = inferer.infer(input.turnSignals, state, false);

    QCOMPARE(goals.primaryGoal.kind, UserGoalKind::InfoQuery);
    QVERIFY(goals.mixedIntent);
    QVERIFY(goals.secondaryGoal.has_value());
}

void SmartIntentV2Tests::plansDeterministicCandidateFromSocialPrefixCommand()
{
    TurnSignalExtractor extractor;
    TurnStateAnalyzer analyzer;
    UserGoalInferer inferer;
    ExecutionIntentPlanner planner;

    const TurnSignals extracted = extractor.extract(QStringLiteral("hey open youtube"));
    TurnStateInput stateInput;
    stateInput.normalizedInput = extracted.normalizedInput;
    stateInput.turnSignals = extracted;
    const TurnState state = analyzer.analyze(stateInput);
    const TurnGoalSet goals = inferer.infer(extracted, state, true);
    AgentTask deterministicTask;
    deterministicTask.type = QStringLiteral("browser_open");
    const QList<ExecutionIntentCandidate> candidates = planner.plan(goals, extracted, true, deterministicTask);

    QVERIFY(!candidates.isEmpty());
    QCOMPARE(candidates.first().route.kind, InputRouteKind::DeterministicTasks);
}

void SmartIntentV2Tests::arbitratesCommandVsInfoConflictToConversation()
{
    TurnSignalExtractor extractor;
    TurnStateAnalyzer analyzer;
    UserGoalInferer inferer;
    ExecutionIntentPlanner planner;
    RouteArbitrator arbitrator;

    const TurnSignals extracted = extractor.extract(QStringLiteral("what does open source mean?"));
    TurnStateInput stateInput;
    stateInput.normalizedInput = extracted.normalizedInput;
    stateInput.turnSignals = extracted;
    const TurnState state = analyzer.analyze(stateInput);
    const TurnGoalSet goals = inferer.infer(extracted, state, false);
    const QList<ExecutionIntentCandidate> candidates = planner.plan(goals, extracted, false, AgentTask{});

    InputRouteDecision policyDecision;
    policyDecision.kind = InputRouteKind::CommandExtraction;
    const RouteArbitrationResult result = arbitrator.arbitrate(
        policyDecision,
        extracted,
        state,
        goals,
        candidates,
        false);

    QCOMPARE(result.decision.kind, InputRouteKind::Conversation);
}

void SmartIntentV2Tests::buildsRouteFinalTracePayload()
{
    RoutingTrace trace;
    trace.rawInput = QStringLiteral("thanks, why is this slow?");
    trace.normalizedInput = QStringLiteral("thanks, why is this slow?");
    trace.turnSignals.hasSmallTalk = true;
    trace.turnSignals.hasQuestionCue = true;
    trace.turnState.isNewTurn = true;
    trace.turnState.reasonCodes = {QStringLiteral("turn_state.new_turn")};
    trace.policyDecision.kind = InputRouteKind::CommandExtraction;
    trace.finalDecision.kind = InputRouteKind::Conversation;
    trace.finalExecutedRoute = QStringLiteral("conversation");
    trace.overridesApplied = {QStringLiteral("arbitrator_override:CommandExtraction->Conversation")};
    trace.reasonCodes = {QStringLiteral("arbitrator.command_vs_info_prefers_info_query")};

    RoutingTraceEmitter emitter;
    const QJsonObject payload = emitter.buildRouteFinalPayload(trace);
    QCOMPARE(payload.value(QStringLiteral("record")).toString(), QStringLiteral("route_final"));
    QCOMPARE(payload.value(QStringLiteral("final_executed_route")).toString(), QStringLiteral("conversation"));
    QVERIFY(payload.contains(QStringLiteral("turn_signals")));
    QVERIFY(payload.contains(QStringLiteral("policy_decision")));
    QVERIFY(payload.contains(QStringLiteral("final_decision")));
}

QTEST_APPLESS_MAIN(SmartIntentV2Tests)
#include "SmartIntentV2Tests.moc"
