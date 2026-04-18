#include <QtTest>
#include <QDateTime>
#include <QSet>

#include "core/ActionRiskPermissionService.h"
#include "core/AssistantBehaviorPolicy.h"
#include "core/InputRouter.h"
#include "core/ToolPermissionRegistry.h"

class AssistantBehaviorPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsMemoryContextLanes();
    void placesConnectorMemoryInActiveCommitments();
    void prioritizesGroundedToolsBeforeRiskyTools();
    void createsActWithProgressSessionForBackgroundTasks();
    void requiresConfirmationForImplicitRiskyActions();
    void requiresConfirmationForPrivateModeSideEffects();
    void quietsProgressDuringFocusedDesktopWork();
    void emitsRiskAndPermissionEventsForPendingConfirmation();
    void usesRegistryBackedPermissionRules();
    void exposesPermissionCapabilitiesForSettingsUi();
    void emitsConfirmationOutcomeEvent();
    void appliesUserPermissionOverrides();
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

void AssistantBehaviorPolicyTests::placesConnectorMemoryInActiveCommitments()
{
    AssistantBehaviorPolicy policy;
    const MemoryContext context = policy.buildMemoryContext(
        QStringLiteral("What should I do with my inbox?"),
        {
            MemoryRecord{
                .type = QStringLiteral("context"),
                .key = QStringLiteral("connector_history_inbox"),
                .value = QStringLiteral("Inbox signals seen 4 times and presented 2 times recently."),
                .confidence = 0.86f,
                .source = QStringLiteral("connector_memory")
            },
            MemoryRecord{
                .type = QStringLiteral("context"),
                .key = QStringLiteral("connector_summary_inbox"),
                .value = QStringLiteral("Inbox activity across 1 sources: seen 4 times, surfaced 2 times, 1 recent source updates."),
                .confidence = 0.9f,
                .source = QStringLiteral("connector_summary")
            }
        });

    QCOMPARE(context.activeCommitments.size(), 2);
    QCOMPARE(context.activeCommitments.first().source, QStringLiteral("connector_memory"));
    QCOMPARE(context.activeCommitments.last().source, QStringLiteral("connector_summary"));
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

    ToolPlan plan;
    plan.sideEffecting = true;
    plan.orderedToolNames = {QStringLiteral("skill_install")};

    const TrustDecision trust = policy.assessTrust(
        QStringLiteral("maybe make that available for me"),
        decision,
        plan);

    QVERIFY(trust.highRisk);
    QVERIFY(trust.requiresConfirmation);
}

void AssistantBehaviorPolicyTests::requiresConfirmationForPrivateModeSideEffects()
{
    AssistantBehaviorPolicy policy;
    InputRouteDecision decision;
    decision.kind = InputRouteKind::BackgroundTasks;
    decision.intent = IntentType::GENERAL_CHAT;

    ToolPlan plan;
    plan.sideEffecting = true;
    plan.orderedToolNames = {QStringLiteral("browser_open")};

    const TrustDecision trust = policy.assessTrust(
        QStringLiteral("open that"),
        decision,
        plan,
        {{QStringLiteral("metadataClass"), QStringLiteral("private_app_only")}});

    QVERIFY(trust.highRisk);
    QVERIFY(trust.requiresConfirmation);
    QCOMPARE(trust.desktopWorkMode, QStringLiteral("private"));
    QCOMPARE(trust.contextReasonCode, QStringLiteral("action_policy.private_mode_confirmation"));
    QVERIFY(trust.userMessage.contains(QStringLiteral("Private Mode")));
}

void AssistantBehaviorPolicyTests::quietsProgressDuringFocusedDesktopWork()
{
    AssistantBehaviorPolicy policy;
    InputRouteDecision decision;
    decision.kind = InputRouteKind::BackgroundTasks;
    decision.intent = IntentType::READ_FILE;

    ToolPlan plan;
    plan.requiresGrounding = true;
    plan.orderedToolNames = {QStringLiteral("file_read")};

    const QVariantMap desktopContext{
        {QStringLiteral("taskId"), QStringLiteral("editor_document")},
        {QStringLiteral("languageHint"), QStringLiteral("cpp")}
    };
    const TrustDecision trust = policy.assessTrust(
        QStringLiteral("read the current file"),
        decision,
        plan,
        desktopContext);
    const ActionSession session = policy.createActionSession(
        QStringLiteral("read the current file"),
        decision,
        plan,
        trust,
        desktopContext);

    QCOMPARE(trust.desktopWorkMode, QStringLiteral("coding"));
    QVERIFY(!session.shouldAnnounceProgress);
}

void AssistantBehaviorPolicyTests::emitsRiskAndPermissionEventsForPendingConfirmation()
{
    ToolPlan plan;
    plan.sideEffecting = true;
    plan.orderedToolNames = {QStringLiteral("file_write")};

    TrustDecision trust;
    trust.highRisk = true;
    trust.requiresConfirmation = true;
    trust.desktopWorkMode = QStringLiteral("coding");
    trust.contextReasonCode = QStringLiteral("action_policy.focused_work_confirmation");

    const QVariantMap desktopContext{
        {QStringLiteral("taskId"), QStringLiteral("editor_document")},
        {QStringLiteral("threadId"), QStringLiteral("desktop::editor_document::vscode::plan_md")}
    };
    const ActionRiskPermissionEvaluation evaluation =
        ActionRiskPermissionService::evaluate(plan, trust, false);

    QCOMPARE(evaluation.risk.level, QStringLiteral("high"));
    QVERIFY(evaluation.risk.confirmationRequired);
    QCOMPARE(evaluation.permissions.size(), 1);
    QVERIFY(!evaluation.permissions.first().granted);
    QCOMPARE(evaluation.permissions.first().capabilityId, QStringLiteral("filesystem_write"));

    const BehaviorTraceEvent riskEvent =
        ActionRiskPermissionService::riskEvent(evaluation, QStringLiteral("BackgroundTasks"), desktopContext);
    const BehaviorTraceEvent permissionEvent =
        ActionRiskPermissionService::permissionEvent(evaluation, QStringLiteral("BackgroundTasks"), desktopContext);

    QCOMPARE(riskEvent.family, QStringLiteral("risk_check"));
    QCOMPARE(riskEvent.threadId, QStringLiteral("desktop::editor_document::vscode::plan_md"));
    QCOMPARE(permissionEvent.family, QStringLiteral("permission"));
    QCOMPARE(permissionEvent.payload.value(QStringLiteral("riskLevel")).toString(), QStringLiteral("high"));
}

void AssistantBehaviorPolicyTests::usesRegistryBackedPermissionRules()
{
    const QList<ToolPermissionRule> fileRules =
        ToolPermissionRegistry::rulesForTool(QStringLiteral("file_patch"));
    const QList<ToolPermissionRule> browserRules =
        ToolPermissionRegistry::rulesForTool(QStringLiteral("browser_open"));

    QCOMPARE(ToolPermissionRegistry::policyVersion(), QStringLiteral("tool_permission_registry.v1"));
    QCOMPARE(fileRules.size(), 1);
    QCOMPARE(fileRules.first().capabilityId, QStringLiteral("filesystem_write"));
    QCOMPARE(browserRules.size(), 1);
    QCOMPARE(browserRules.first().capabilityId, QStringLiteral("desktop_automation"));

    ToolPlan plan;
    plan.orderedToolNames = {QStringLiteral("file_patch"), QStringLiteral("browser_open")};
    const ActionRiskPermissionEvaluation evaluation =
        ActionRiskPermissionService::evaluate(plan, TrustDecision{}, true);

    QCOMPARE(evaluation.permissions.size(), 2);
    QCOMPARE(evaluation.risk.details.value(QStringLiteral("permissionRegistryVersion")).toString(),
             QStringLiteral("tool_permission_registry.v1"));
    QVERIFY(evaluation.permissions.at(0).granted);
    QCOMPARE(evaluation.permissions.at(0).reasonCode, QStringLiteral("permission.allowed_by_registry"));
}

void AssistantBehaviorPolicyTests::exposesPermissionCapabilitiesForSettingsUi()
{
    const QList<PermissionCapabilityInfo> capabilities =
        ToolPermissionRegistry::capabilityOptions();

    QVERIFY(capabilities.size() >= 5);
    QCOMPARE(capabilities.first().capabilityId, QStringLiteral("filesystem_write"));
    QVERIFY(!capabilities.first().label.isEmpty());
    QVERIFY(!capabilities.first().description.isEmpty());

    QSet<QString> capabilityIds;
    for (const PermissionCapabilityInfo &capability : capabilities) {
        QVERIFY(!capability.capabilityId.trimmed().isEmpty());
        QVERIFY(!capabilityIds.contains(capability.capabilityId));
        capabilityIds.insert(capability.capabilityId);
    }
    QVERIFY(capabilityIds.contains(QStringLiteral("network_grounding")));
}

void AssistantBehaviorPolicyTests::emitsConfirmationOutcomeEvent()
{
    ToolPlan plan;
    plan.sideEffecting = true;
    plan.orderedToolNames = {QStringLiteral("file_patch")};

    TrustDecision trust;
    trust.highRisk = true;
    trust.requiresConfirmation = true;
    trust.contextReasonCode = QStringLiteral("action_policy.focused_work_confirmation");

    ActionSession session;
    session.id = QStringLiteral("session-123");
    session.userRequest = QStringLiteral("patch that file");
    session.toolPlan = plan;
    session.trust = trust;

    const ActionRiskPermissionEvaluation evaluation =
        ActionRiskPermissionService::evaluate(plan, trust, false);
    const BehaviorTraceEvent event =
        ActionRiskPermissionService::confirmationOutcomeEvent(
            evaluation,
            QStringLiteral("BackgroundTasks"),
            session,
            QStringLiteral("approved"),
            QStringLiteral("yes, continue"),
            {{QStringLiteral("threadId"), QStringLiteral("desktop::editor_document::vscode::plan_md")}});

    QCOMPARE(event.family, QStringLiteral("confirmation"));
    QCOMPARE(event.stage, QStringLiteral("approved"));
    QCOMPARE(event.reasonCode, QStringLiteral("confirmation.approved"));
    QCOMPARE(event.sessionId, QStringLiteral("session-123"));
    QVERIFY(event.payload.value(QStringLiteral("executionWillContinue")).toBool());
    QCOMPARE(event.payload.value(QStringLiteral("riskReasonCode")).toString(),
             QStringLiteral("action_policy.focused_work_confirmation"));
}

void AssistantBehaviorPolicyTests::appliesUserPermissionOverrides()
{
    ToolPlan plan;
    plan.sideEffecting = true;
    plan.orderedToolNames = {
        QStringLiteral("file_patch"),
        QStringLiteral("browser_open")
    };

    TrustDecision trust;
    trust.highRisk = true;
    trust.requiresConfirmation = true;

    const ActionRiskPermissionEvaluation evaluation =
        ActionRiskPermissionService::evaluate(
            plan,
            trust,
            false,
            {
                PermissionOverrideRule{
                    .capabilityId = QStringLiteral("filesystem_write"),
                    .decision = QStringLiteral("allow"),
                    .scope = QStringLiteral("project_workspace"),
                    .reasonCode = QStringLiteral("permission.allowed_by_user_override")
                },
                PermissionOverrideRule{
                    .capabilityId = QStringLiteral("desktop_automation"),
                    .decision = QStringLiteral("deny"),
                    .scope = QStringLiteral("blocked"),
                    .reasonCode = QStringLiteral("permission.denied_by_user_override")
                }
            });

    QCOMPARE(evaluation.permissions.size(), 2);
    QVERIFY(evaluation.permissions.at(0).granted);
    QCOMPARE(evaluation.permissions.at(0).scope, QStringLiteral("project_workspace"));
    QCOMPARE(evaluation.permissions.at(0).reasonCode, QStringLiteral("permission.allowed_by_user_override"));
    QVERIFY(!evaluation.permissions.at(1).granted);
    QCOMPARE(evaluation.permissions.at(1).reasonCode, QStringLiteral("permission.denied_by_user_override"));
    QCOMPARE(evaluation.risk.details.value(QStringLiteral("permissionOverrideCount")).toInt(), 2);
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
