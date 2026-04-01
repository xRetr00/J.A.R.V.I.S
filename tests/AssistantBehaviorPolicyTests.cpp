#include <QtTest>
#include <QDateTime>

#include "core/AssistantBehaviorPolicy.h"
#include "core/InputRouter.h"

class AssistantBehaviorPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsMemoryContextLanes();
    void prioritizesGroundedToolsBeforeRiskyTools();
    void createsActWithProgressSessionForBackgroundTasks();
    void requiresConfirmationForImplicitRiskyActions();
    void decidesRouteFromPolicyContext();
    void routesHighConfidenceToolIntentToAgent();
    void acceptsExplicitConfirmationReply();
    void recognizesRejectionReply();
    void continuesReferentialFollowUpAgainstRecentActionThread();
    void ignoresFreshRequestAgainstRecentActionThread();
};

void AssistantBehaviorPolicyTests::buildsMemoryContextLanes()
{
    AssistantBehaviorPolicy policy;
    const MemoryContext context = policy.buildMemoryContext(
        QStringLiteral("What am I working on?"),
        {
            MemoryRecord{.type = QStringLiteral("preference"), .key = QStringLiteral("general_preference"), .value = QStringLiteral("short answers")},
            MemoryRecord{.type = QStringLiteral("context"), .key = QStringLiteral("current_project"), .value = QStringLiteral("Vaxil behavior refactor")},
            MemoryRecord{.type = QStringLiteral("fact"), .key = QStringLiteral("recent_note"), .value = QStringLiteral("Need to revisit tool narration")}
        });

    QCOMPARE(context.profile.size(), 1);
    QCOMPARE(context.activeCommitments.size(), 1);
    QCOMPARE(context.episodic.size(), 1);
}

void AssistantBehaviorPolicyTests::prioritizesGroundedToolsBeforeRiskyTools()
{
    AssistantBehaviorPolicy policy;
    const QList<AgentToolSpec> selected = policy.selectRelevantTools(
        QStringLiteral("check the latest release notes and summarize them"),
        IntentType::GENERAL_CHAT,
        {
            {QStringLiteral("file_write"), {}, {}},
            {QStringLiteral("web_search"), {}, {}},
            {QStringLiteral("browser_open"), {}, {}},
            {QStringLiteral("memory_search"), {}, {}}
        });

    QVERIFY(!selected.isEmpty());
    QCOMPARE(selected.first().name, QStringLiteral("web_search"));
}

void AssistantBehaviorPolicyTests::createsActWithProgressSessionForBackgroundTasks()
{
    AssistantBehaviorPolicy policy;
    InputRouteDecision decision;
    decision.kind = InputRouteKind::BackgroundTasks;
    decision.intent = IntentType::READ_FILE;

    const ToolPlan plan = policy.buildToolPlan(
        QStringLiteral("read the startup log"),
        IntentType::READ_FILE,
        {
            {QStringLiteral("file_read"), {}, {}},
            {QStringLiteral("log_tail"), {}, {}}
        });
    const TrustDecision trust = policy.assessTrust(QStringLiteral("read the startup log"), decision, plan);
    const ActionSession session = policy.createActionSession(
        QStringLiteral("read the startup log"),
        decision,
        plan,
        trust);

    QCOMPARE(session.responseMode, ResponseMode::ActWithProgress);
    QVERIFY(!session.preamble.isEmpty());
    QVERIFY(session.shouldAnnounceProgress);
}

void AssistantBehaviorPolicyTests::requiresConfirmationForImplicitRiskyActions()
{
    AssistantBehaviorPolicy policy;
    InputRouteDecision decision;
    decision.kind = InputRouteKind::BackgroundTasks;
    decision.intent = IntentType::GENERAL_CHAT;

    const ToolPlan plan = policy.buildToolPlan(
        QStringLiteral("maybe make that available for me"),
        IntentType::GENERAL_CHAT,
        {
            {QStringLiteral("skill_install"), {}, {}},
            {QStringLiteral("web_search"), {}, {}}
        });
    const TrustDecision trust = policy.assessTrust(
        QStringLiteral("maybe make that available for me"),
        decision,
        plan);

    QVERIFY(trust.highRisk);
    QVERIFY(trust.requiresConfirmation);
}

void AssistantBehaviorPolicyTests::decidesRouteFromPolicyContext()
{
    AssistantBehaviorPolicy policy;
    InputRouterContext context;
    context.agentEnabled = true;
    context.explicitWebSearch = true;
    context.explicitWebQuery = QStringLiteral("latest Vaxil release notes");

    const InputRouteDecision decision = policy.decideRoute(context);
    QCOMPARE(decision.kind, InputRouteKind::BackgroundTasks);
    QCOMPARE(decision.tasks.size(), 1);
    QCOMPARE(decision.tasks.first().type, QStringLiteral("web_search"));
}

void AssistantBehaviorPolicyTests::routesHighConfidenceToolIntentToAgent()
{
    AssistantBehaviorPolicy policy;
    InputRouterContext context;
    context.agentEnabled = true;
    context.aiAvailable = true;
    context.effectiveIntent = IntentType::WRITE_FILE;
    context.effectiveIntentConfidence = 0.93f;

    const InputRouteDecision decision = policy.decideRoute(context);
    QCOMPARE(decision.kind, InputRouteKind::AgentConversation);
    QCOMPARE(decision.intent, IntentType::WRITE_FILE);
}

void AssistantBehaviorPolicyTests::acceptsExplicitConfirmationReply()
{
    AssistantBehaviorPolicy policy;
    ActionSession session;
    session.userRequest = QStringLiteral("open the browser and install that skill");

    QVERIFY(policy.isConfirmationReply(QStringLiteral("yes, go ahead"), session));
    QVERIFY(policy.isConfirmationReply(QStringLiteral("open it"), session));
}

void AssistantBehaviorPolicyTests::recognizesRejectionReply()
{
    AssistantBehaviorPolicy policy;
    QVERIFY(policy.isRejectionReply(QStringLiteral("no, cancel that")));
    QVERIFY(!policy.isRejectionReply(QStringLiteral("tell me more")));
}

void AssistantBehaviorPolicyTests::continuesReferentialFollowUpAgainstRecentActionThread()
{
    AssistantBehaviorPolicy policy;
    InputRouteDecision decision;
    decision.kind = InputRouteKind::CommandExtraction;

    ActionThread thread;
    thread.taskType = QStringLiteral("web_search");
    thread.userGoal = QStringLiteral("Find the latest Tesla news");
    thread.resultSummary = QStringLiteral("Found several current sources.");
    thread.state = ActionThreadState::Completed;
    thread.valid = true;
    thread.expiresAtMs = QDateTime::currentMSecsSinceEpoch() + 30000;

    QVERIFY(policy.shouldContinueActionThread(
        QStringLiteral("open it"),
        decision,
        thread,
        QDateTime::currentMSecsSinceEpoch()));
}

void AssistantBehaviorPolicyTests::ignoresFreshRequestAgainstRecentActionThread()
{
    AssistantBehaviorPolicy policy;
    InputRouteDecision decision;
    decision.kind = InputRouteKind::BackgroundTasks;

    ActionThread thread;
    thread.taskType = QStringLiteral("file_read");
    thread.userGoal = QStringLiteral("Read the startup log");
    thread.resultSummary = QStringLiteral("Read completed.");
    thread.state = ActionThreadState::Completed;
    thread.valid = true;
    thread.expiresAtMs = QDateTime::currentMSecsSinceEpoch() + 30000;

    QVERIFY(!policy.shouldContinueActionThread(
        QStringLiteral("search for the latest AMD news"),
        decision,
        thread,
        QDateTime::currentMSecsSinceEpoch()));
}

QTEST_APPLESS_MAIN(AssistantBehaviorPolicyTests)
#include "AssistantBehaviorPolicyTests.moc"
