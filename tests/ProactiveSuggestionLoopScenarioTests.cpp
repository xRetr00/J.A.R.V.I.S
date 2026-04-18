#include <QtTest>

#include "cognition/ProactiveCooldownTracker.h"
#include "cognition/ProactiveSuggestionPlanner.h"

class ProactiveSuggestionLoopScenarioTests : public QObject
{
    Q_OBJECT

private slots:
    void connectorSuggestionPresentThenDuplicateSuppress();
    void focusedDesktopWorkSuppressesNonCriticalSuggestion();
};

namespace {
QVariantMap scheduleMetadata()
{
    return {
        {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
        {QStringLiteral("eventTitle"), QStringLiteral("Sprint planning")},
        {QStringLiteral("eventStartUtc"), QStringLiteral("2026-04-18T15:00:00.000Z")},
        {QStringLiteral("occurredAtUtc"), QStringLiteral("2026-04-18T14:58:00.000Z")},
        {QStringLiteral("taskKey"), QStringLiteral("schedule:team")}
    };
}
}

void ProactiveSuggestionLoopScenarioTests::connectorSuggestionPresentThenDuplicateSuppress()
{
    const qint64 nowMs = QDateTime::fromString(QStringLiteral("2026-04-18T15:00:00.000Z"),
                                               Qt::ISODateWithMs).toMSecsSinceEpoch();

    const ProactiveSuggestionPlan first = ProactiveSuggestionPlanner::plan({
        .sourceKind = QStringLiteral("connector_schedule_calendar"),
        .taskType = QStringLiteral("live_update"),
        .resultSummary = QStringLiteral("Schedule updated: Sprint planning"),
        .sourceUrls = {},
        .sourceMetadata = scheduleMetadata(),
        .presentationKey = QStringLiteral("connector:schedule:team"),
        .lastPresentedKey = QString(),
        .lastPresentedAtMs = 0,
        .success = true,
        .cooldownState = CooldownState{},
        .focusMode = FocusModeState{},
        .nowMs = nowMs
    });

    QVERIFY(first.decision.allowed);
    QCOMPARE(first.selectedProposal.capabilityId, QStringLiteral("schedule_follow_up"));
    QCOMPARE(first.selectedProposal.arguments.value(QStringLiteral("sourceLabel")).toString(),
             QStringLiteral("Sprint planning"));
    QCOMPARE(first.selectedProposal.arguments.value(QStringLiteral("presentationKeyHint")).toString(),
             QStringLiteral("schedule:team"));
    QVERIFY(!first.selectedSummary.isEmpty());

    const ProactiveCooldownCommit commit = ProactiveCooldownTracker::commitPresentedSurface({
        .state = CooldownState{},
        .desktopContext = {},
        .taskType = QStringLiteral("live_update"),
        .surfaceKind = QStringLiteral("connector_event_toast"),
        .priority = QStringLiteral("medium"),
        .nowMs = nowMs
    });

    QVERIFY(commit.nextState.isActive(nowMs + 30000));
    QVERIFY(commit.nextState.threadId.contains(QStringLiteral("connector_event_toast")));

    const ProactiveSuggestionPlan duplicate = ProactiveSuggestionPlanner::plan({
        .sourceKind = QStringLiteral("connector_schedule_calendar"),
        .taskType = QStringLiteral("live_update"),
        .resultSummary = QStringLiteral("Schedule updated: Sprint planning"),
        .sourceUrls = {},
        .sourceMetadata = scheduleMetadata(),
        .presentationKey = QStringLiteral("connector:schedule:team"),
        .lastPresentedKey = QStringLiteral("connector:schedule:team"),
        .lastPresentedAtMs = nowMs,
        .success = true,
        .cooldownState = commit.nextState,
        .focusMode = FocusModeState{},
        .nowMs = nowMs + 30000
    });

    QVERIFY(!duplicate.decision.allowed);
    QCOMPARE(duplicate.rankedProposals.first().reasonCode,
             QStringLiteral("proposal_rank.recent_duplicate_penalty"));
    QCOMPARE(duplicate.cooldownDecision.reasonCode, QStringLiteral("cooldown.low_novelty"));
    QVERIFY(duplicate.selectedSummary.isEmpty());
}

void ProactiveSuggestionLoopScenarioTests::focusedDesktopWorkSuppressesNonCriticalSuggestion()
{
    const ProactiveSuggestionPlan plan = ProactiveSuggestionPlanner::plan({
        .sourceKind = QStringLiteral("desktop_context"),
        .taskType = QStringLiteral("active_window"),
        .resultSummary = QStringLiteral("User is editing MainWindow.cpp"),
        .sourceUrls = {},
        .sourceMetadata = {},
        .presentationKey = QString(),
        .lastPresentedKey = QString(),
        .lastPresentedAtMs = 0,
        .success = true,
        .desktopContext = {
            {QStringLiteral("taskId"), QStringLiteral("editor_document")},
            {QStringLiteral("threadId"), QStringLiteral("editor::vaxil")},
            {QStringLiteral("documentContext"), QStringLiteral("MainWindow.cpp")},
            {QStringLiteral("workspaceContext"), QStringLiteral("Vaxil")}
        },
        .desktopContextAtMs = 1000,
        .cooldownState = CooldownState{},
        .focusMode = FocusModeState{},
        .nowMs = 1500
    });

    QVERIFY(!plan.decision.allowed);
    QCOMPARE(plan.decision.reasonCode, QStringLiteral("proposal.focused_context_suppressed"));
    QCOMPARE(plan.selectedProposal.arguments.value(QStringLiteral("sourceLabel")).toString(),
             QStringLiteral("MainWindow.cpp"));
    QVERIFY(plan.selectedSummary.isEmpty());
}

QTEST_APPLESS_MAIN(ProactiveSuggestionLoopScenarioTests)
#include "ProactiveSuggestionLoopScenarioTests.moc"
