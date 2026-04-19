#include <QtTest>

#include <QSqlDatabase>
#include <QTemporaryDir>

#include "cognition/CooldownEngine.h"
#include "cognition/ProactiveSuggestionPlanner.h"
#include "core/ActionRiskPermissionService.h"
#include "telemetry/BehavioralEventLedger.h"

class ProactiveActionFocusModePolicyRegressionTests : public QObject
{
    Q_OBJECT

private slots:
    void focusModeCooccurrenceCoversCriticalVsNonCriticalWithActionGates();
    void policyRegressionKeepsBreakAndNoveltyReasonCodesStable();
};

namespace {
QVariantMap mixedDesktopConnectorContext()
{
    return {
        {QStringLiteral("threadId"), QStringLiteral("desktop::editor_document::calendar::sprint_plan")},
        {QStringLiteral("taskId"), QStringLiteral("editor_document")},
        {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
        {QStringLiteral("eventTitle"), QStringLiteral("Sprint planning")}
    };
}

ToolPlan sideEffectingPlan()
{
    ToolPlan plan;
    plan.sideEffecting = true;
    plan.orderedToolNames = {QStringLiteral("file_patch"), QStringLiteral("web_search")};
    return plan;
}

TrustDecision highRiskTrust()
{
    TrustDecision trust;
    trust.highRisk = true;
    trust.requiresConfirmation = true;
    trust.contextReasonCode = QStringLiteral("action_policy.mixed_context_confirmation");
    trust.desktopWorkMode = QStringLiteral("focused");
    trust.reason = QStringLiteral("Mixed editor + connector context");
    return trust;
}
}

void ProactiveActionFocusModePolicyRegressionTests::focusModeCooccurrenceCoversCriticalVsNonCriticalWithActionGates()
{
    if (!QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE"))) {
        QSKIP("QSQLITE driver is not available in this runtime.");
    }
    const QVariantMap desktopContext = mixedDesktopConnectorContext();
    const ActionRiskPermissionEvaluation pending = ActionRiskPermissionService::evaluate(
        sideEffectingPlan(), highRiskTrust(), false, {});

    struct FocusRow {
        QString priority;
        bool allowCriticalAlerts;
        QString expectedReasonCode;
        QString confirmationOutcome;
    };
    const QList<FocusRow> rows = {
        {QStringLiteral("medium"), true, QStringLiteral("focus_mode.suppressed"), QStringLiteral("denied")},
        {QStringLiteral("critical"), false, QStringLiteral("focus_mode.critical_blocked"), QStringLiteral("canceled")},
        {QStringLiteral("critical"), true, QStringLiteral("cooldown.break_high_novelty"), QStringLiteral("approved")}
    };

    for (int i = 0; i < rows.size(); ++i) {
        const FocusRow row = rows.at(i);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        BehavioralEventLedger ledger(dir.path(), true);
        QVERIFY(ledger.initialize());

        const qint64 nowMs = QDateTime::fromString(QStringLiteral("2026-04-18T22:00:00.000Z"),
                                                   Qt::ISODateWithMs).toMSecsSinceEpoch() + (i * 1000);
        CooldownEngine cooldownEngine;
        const BehaviorDecision decision = cooldownEngine.evaluate({
            .context = CompanionContextSnapshot{
                .threadId = ContextThreadId{QStringLiteral("focus_mode::row_%1").arg(i)},
                .appId = QStringLiteral("calendar"),
                .taskId = QStringLiteral("meeting_alert"),
                .topic = QStringLiteral("deadline"),
                .recentIntent = QStringLiteral("notify"),
                .confidence = 0.90
            },
            .state = CooldownState{
                .threadId = QStringLiteral("connector_event_toast::live_update"),
                .activeUntilEpochMs = nowMs + 90000
            },
            .focusMode = FocusModeState{.enabled = true, .allowCriticalAlerts = row.allowCriticalAlerts},
            .priority = row.priority,
            .confidence = 0.86,
            .novelty = 0.80,
            .nowMs = nowMs
        });
        QCOMPARE(decision.reasonCode, row.expectedReasonCode);

        ActionSession session;
        session.id = QStringLiteral("session_focus_mode_row_%1").arg(i);
        session.userRequest = QStringLiteral("focus mode cooccurrence");
        const ActionRiskPermissionEvaluation approved = ActionRiskPermissionService::evaluate(
            sideEffectingPlan(), highRiskTrust(), true, {});
        const ActionRiskPermissionEvaluation evalForOutcome = row.confirmationOutcome == QStringLiteral("approved")
            ? approved
            : pending;

        BehaviorTraceEvent entry = BehaviorTraceEvent::create(
            QStringLiteral("action_proposal"),
            QStringLiteral("gated"),
            decision.reasonCode,
            {
                {QStringLiteral("action"), decision.action},
                {QStringLiteral("priority"), row.priority},
                {QStringLiteral("focusModeEnabled"), true},
                {QStringLiteral("allowCriticalAlerts"), row.allowCriticalAlerts}
            });
        BehaviorTraceEvent permission = ActionRiskPermissionService::permissionEvent(
            pending, QStringLiteral("FocusModeCooccurrence"), desktopContext);
        BehaviorTraceEvent confirmation = ActionRiskPermissionService::confirmationOutcomeEvent(
            evalForOutcome,
            QStringLiteral("FocusModeCooccurrence"),
            session,
            row.confirmationOutcome,
            row.confirmationOutcome,
            desktopContext);

        const QString traceId = QStringLiteral("trace_focus_mode_cooccurrence_%1").arg(i);
        entry.traceId = traceId;
        permission.traceId = traceId;
        confirmation.traceId = traceId;
        entry.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
        entry.timestampUtc = QDateTime::fromMSecsSinceEpoch(nowMs, Qt::UTC);
        permission.timestampUtc = entry.timestampUtc.addMSecs(1);
        confirmation.timestampUtc = entry.timestampUtc.addMSecs(2);

        QVERIFY(ledger.recordEvent(entry));
        QVERIFY(ledger.recordEvent(permission));
        QVERIFY(ledger.recordEvent(confirmation));
        const QList<BehaviorTraceEvent> events = ledger.recentEvents(8);
        QCOMPARE(events.size(), 3);
        QCOMPARE(events.first().reasonCode, row.expectedReasonCode);
        QCOMPARE(events.last().stage, row.confirmationOutcome);
        QCOMPARE(events.last().payload.value(QStringLiteral("executionWillContinue")).toBool(),
                 row.confirmationOutcome == QStringLiteral("approved"));
    }
}

void ProactiveActionFocusModePolicyRegressionTests::policyRegressionKeepsBreakAndNoveltyReasonCodesStable()
{
    const qint64 nowMs = QDateTime::fromString(QStringLiteral("2026-04-18T22:20:00.000Z"),
                                               Qt::ISODateWithMs).toMSecsSinceEpoch();
    const QList<QVariantMap> noveltyVariants = {
        {
            {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
            {QStringLiteral("eventTitle"), QStringLiteral("Sprint planning")},
            {QStringLiteral("occurredAtUtc"), QStringLiteral("2026-04-18T08:00:00.000Z")},
            {QStringLiteral("historySeenCount"), 5},
            {QStringLiteral("connectorKindRecentSeenCount"), 5},
            {QStringLiteral("connectorKindRecentPresentedCount"), 2}
        },
        {
            {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
            {QStringLiteral("eventTitle"), QStringLiteral("Sprint planning updated")},
            {QStringLiteral("occurredAtUtc"), QStringLiteral("2026-04-18T08:15:00.000Z")},
            {QStringLiteral("historySeenCount"), 6},
            {QStringLiteral("connectorKindRecentSeenCount"), 5},
            {QStringLiteral("connectorKindRecentPresentedCount"), 3}
        }
    };
    for (const QVariantMap &metadata : noveltyVariants) {
        const ProactiveSuggestionPlan plan = ProactiveSuggestionPlanner::plan({
            .sourceKind = QStringLiteral("connector_schedule_calendar"),
            .taskType = QStringLiteral("live_update"),
            .resultSummary = QStringLiteral("Schedule updated: Sprint planning"),
            .sourceUrls = {},
            .sourceMetadata = metadata,
            .success = true,
            .cooldownState = CooldownState{
                .threadId = QStringLiteral("connector_event_toast::live_update"),
                .activeUntilEpochMs = nowMs + 120000
            },
            .focusMode = FocusModeState{},
            .nowMs = nowMs
        });
        QVERIFY(!plan.decision.allowed);
        QCOMPARE(plan.decision.reasonCode, QStringLiteral("cooldown.low_novelty"));
    }

    CooldownEngine cooldownEngine;
    const QList<QPair<double, double>> breakVariants = {{0.86, 0.80}, {0.90, 0.76}, {0.80, 0.90}};
    for (const auto &scores : breakVariants) {
        const BehaviorDecision decision = cooldownEngine.evaluate({
            .context = CompanionContextSnapshot{
                .threadId = ContextThreadId{QStringLiteral("connector_event_toast::live_update")},
                .appId = QStringLiteral("calendar"),
                .taskId = QStringLiteral("meeting_alert")
            },
            .state = CooldownState{
                .threadId = QStringLiteral("connector_event_toast::live_update"),
                .activeUntilEpochMs = nowMs + 90000
            },
            .focusMode = FocusModeState{},
            .priority = QStringLiteral("high"),
            .confidence = scores.first,
            .novelty = scores.second,
            .nowMs = nowMs
        });
        QVERIFY(decision.allowed);
        QCOMPARE(decision.action, QStringLiteral("break_cooldown"));
        QCOMPARE(decision.reasonCode, QStringLiteral("cooldown.break_high_novelty"));
    }
}

QTEST_APPLESS_MAIN(ProactiveActionFocusModePolicyRegressionTests)
#include "ProactiveActionFocusModePolicyRegressionTests.moc"
