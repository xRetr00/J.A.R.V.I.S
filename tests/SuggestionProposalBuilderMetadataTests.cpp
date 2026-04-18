#include <QtTest>

#include "cognition/ProactiveSuggestionGate.h"
#include "cognition/ProactiveSuggestionPlanner.h"
#include "cognition/SuggestionProposalBuilder.h"
#include "cognition/SuggestionProposalRanker.h"

class SuggestionProposalBuilderMetadataTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsEditorProposalFromDesktopMetadata();
    void buildsBrowserProposalFromDesktopMetadata();
    void buildsConnectorProposalFromMetadata();
    void attachesPresentationHintsFromMetadata();
    void rankerPenalizesDuplicatePresentationHint();
    void plannerLowersNoveltyForDuplicatePresentationHint();
    void gateExposesProposalEvidence();
    void plannerPassesSourceMetadataIntoBuilder();
};

namespace {
bool hasCapability(const QList<ActionProposal> &proposals, const QString &capabilityId)
{
    for (const ActionProposal &proposal : proposals) {
        if (proposal.capabilityId == capabilityId) {
            return true;
        }
    }
    return false;
}

ActionProposal proposalFor(const QList<ActionProposal> &proposals, const QString &capabilityId)
{
    for (const ActionProposal &proposal : proposals) {
        if (proposal.capabilityId == capabilityId) {
            return proposal;
        }
    }
    return {};
}
}

void SuggestionProposalBuilderMetadataTests::buildsEditorProposalFromDesktopMetadata()
{
    const QList<ActionProposal> proposals = SuggestionProposalBuilder::build({
        .sourceKind = QStringLiteral("desktop_context"),
        .taskType = QStringLiteral("active_window"),
        .resultSummary = QStringLiteral("User is editing MainWindow.cpp"),
        .sourceUrls = {},
        .sourceMetadata = {
            {QStringLiteral("taskId"), QStringLiteral("editor_document")},
            {QStringLiteral("documentContext"), QStringLiteral("MainWindow.cpp")},
            {QStringLiteral("workspaceContext"), QStringLiteral("Vaxil")}
        },
        .success = true
    });

    QVERIFY(hasCapability(proposals, QStringLiteral("document_follow_up")));
    QCOMPARE(proposalFor(proposals, QStringLiteral("document_follow_up")).title,
             QStringLiteral("Help with MainWindow.cpp"));
}

void SuggestionProposalBuilderMetadataTests::buildsBrowserProposalFromDesktopMetadata()
{
    const QList<ActionProposal> proposals = SuggestionProposalBuilder::build({
        .sourceKind = QStringLiteral("desktop_context"),
        .taskType = QStringLiteral("active_window"),
        .resultSummary = QStringLiteral("User is reading an API reference page"),
        .sourceUrls = {},
        .sourceMetadata = {
            {QStringLiteral("taskId"), QStringLiteral("browser_tab")},
            {QStringLiteral("documentContext"), QStringLiteral("Qt Test Reference")},
            {QStringLiteral("siteContext"), QStringLiteral("doc.qt.io")}
        },
        .success = true
    });

    QVERIFY(hasCapability(proposals, QStringLiteral("browser_follow_up")));
}

void SuggestionProposalBuilderMetadataTests::buildsConnectorProposalFromMetadata()
{
    const QList<ActionProposal> proposals = SuggestionProposalBuilder::build({
        .sourceKind = QStringLiteral("connector_event"),
        .taskType = QStringLiteral("live_update"),
        .resultSummary = QStringLiteral("Maildrop received a GitHub review request"),
        .sourceUrls = {},
        .sourceMetadata = {
            {QStringLiteral("connectorKind"), QStringLiteral("inbox")},
            {QStringLiteral("producerId"), QStringLiteral("maildrop")},
            {QStringLiteral("sender"), QStringLiteral("GitHub")},
            {QStringLiteral("subject"), QStringLiteral("Review requested")}
        },
        .success = true
    });

    QVERIFY(hasCapability(proposals, QStringLiteral("inbox_follow_up")));
}

void SuggestionProposalBuilderMetadataTests::attachesPresentationHintsFromMetadata()
{
    const QList<ActionProposal> proposals = SuggestionProposalBuilder::build({
        .sourceKind = QStringLiteral("connector_event"),
        .taskType = QStringLiteral("live_update"),
        .resultSummary = QStringLiteral("Calendar received sprint planning"),
        .sourceUrls = {},
        .sourceMetadata = {
            {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
            {QStringLiteral("eventTitle"), QStringLiteral("Sprint planning")},
            {QStringLiteral("eventStartUtc"), QStringLiteral("2026-04-18T15:00:00.000Z")},
            {QStringLiteral("taskKey"), QStringLiteral("schedule:team")}
        },
        .success = true
    });

    ActionProposal schedule;
    for (const ActionProposal &proposal : proposals) {
        if (proposal.capabilityId == QStringLiteral("schedule_follow_up")) {
            schedule = proposal;
            break;
        }
    }

    QCOMPARE(schedule.title, QStringLiteral("Review Sprint planning"));
    QCOMPARE(schedule.arguments.value(QStringLiteral("proposalReasonCode")).toString(),
             QStringLiteral("proposal.metadata.schedule"));
    QCOMPARE(schedule.arguments.value(QStringLiteral("presentationKeyHint")).toString(),
             QStringLiteral("schedule:team"));
    QCOMPARE(schedule.arguments.value(QStringLiteral("sourceLabel")).toString(),
             QStringLiteral("Sprint planning"));
}

void SuggestionProposalBuilderMetadataTests::rankerPenalizesDuplicatePresentationHint()
{
    const QList<ActionProposal> proposals = SuggestionProposalBuilder::build({
        .sourceKind = QStringLiteral("connector_event"),
        .taskType = QStringLiteral("live_update"),
        .resultSummary = QStringLiteral("Calendar received sprint planning"),
        .sourceUrls = {},
        .sourceMetadata = {
            {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
            {QStringLiteral("eventTitle"), QStringLiteral("Sprint planning")},
            {QStringLiteral("taskKey"), QStringLiteral("schedule:team")}
        },
        .success = true
    });

    const QList<RankedSuggestionProposal> ranked = SuggestionProposalRanker::rank({
        .proposals = proposals,
        .sourceKind = QStringLiteral("connector_event"),
        .taskType = QStringLiteral("live_update"),
        .sourceMetadata = {{QStringLiteral("connectorKind"), QStringLiteral("schedule")}},
        .presentationKey = QString(),
        .lastPresentedKey = QStringLiteral("schedule:team"),
        .lastPresentedAtMs = 1000,
        .cooldownState = CooldownState{},
        .focusMode = FocusModeState{},
        .nowMs = 1500
    });

    QVERIFY(!ranked.isEmpty());
    QCOMPARE(ranked.first().reasonCode, QStringLiteral("proposal_rank.recent_duplicate_penalty"));
}

void SuggestionProposalBuilderMetadataTests::plannerLowersNoveltyForDuplicatePresentationHint()
{
    const ProactiveSuggestionPlan plan = ProactiveSuggestionPlanner::plan({
        .sourceKind = QStringLiteral("connector_event"),
        .taskType = QStringLiteral("live_update"),
        .resultSummary = QStringLiteral("Calendar received sprint planning"),
        .sourceUrls = {},
        .sourceMetadata = {
            {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
            {QStringLiteral("eventTitle"), QStringLiteral("Sprint planning")},
            {QStringLiteral("taskKey"), QStringLiteral("schedule:team")}
        },
        .presentationKey = QString(),
        .lastPresentedKey = QStringLiteral("schedule:team"),
        .lastPresentedAtMs = 1000,
        .success = true,
        .cooldownState = CooldownState{},
        .focusMode = FocusModeState{},
        .nowMs = 1500
    });

    QVERIFY(!plan.rankedProposals.isEmpty());
    QVERIFY(plan.noveltyScore < 0.30);
    QCOMPARE(plan.rankedProposals.first().reasonCode, QStringLiteral("proposal_rank.recent_duplicate_penalty"));
}

void SuggestionProposalBuilderMetadataTests::gateExposesProposalEvidence()
{
    ActionProposal proposal;
    proposal.proposalId = QStringLiteral("p1");
    proposal.capabilityId = QStringLiteral("schedule_follow_up");
    proposal.title = QStringLiteral("Review Sprint planning");
    proposal.priority = QStringLiteral("medium");
    proposal.arguments = {
        {QStringLiteral("proposalReasonCode"), QStringLiteral("proposal.metadata.schedule")},
        {QStringLiteral("sourceLabel"), QStringLiteral("Sprint planning")},
        {QStringLiteral("presentationKeyHint"), QStringLiteral("schedule:team")}
    };

    const BehaviorDecision decision = ProactiveSuggestionGate::evaluate({
        .proposal = proposal,
        .proposalScore = 0.7,
        .focusMode = FocusModeState{},
        .nowMs = 1500
    });

    QCOMPARE(decision.details.value(QStringLiteral("proposalReasonCode")).toString(),
             QStringLiteral("proposal.metadata.schedule"));
    QCOMPARE(decision.details.value(QStringLiteral("sourceLabel")).toString(),
             QStringLiteral("Sprint planning"));
    QCOMPARE(decision.details.value(QStringLiteral("presentationKeyHint")).toString(),
             QStringLiteral("schedule:team"));
}

void SuggestionProposalBuilderMetadataTests::plannerPassesSourceMetadataIntoBuilder()
{
    const ProactiveSuggestionPlan plan = ProactiveSuggestionPlanner::plan({
        .sourceKind = QStringLiteral("desktop_context"),
        .taskType = QStringLiteral("active_window"),
        .resultSummary = QStringLiteral("User is editing a project file"),
        .sourceUrls = {},
        .sourceMetadata = {{QStringLiteral("taskId"), QStringLiteral("editor_document")}},
        .success = true,
        .desktopContext = {
            {QStringLiteral("taskId"), QStringLiteral("editor_document")},
            {QStringLiteral("threadId"), QStringLiteral("editor::project")}
        },
        .desktopContextAtMs = 1000,
        .cooldownState = CooldownState{},
        .focusMode = FocusModeState{},
        .nowMs = 1500
    });

    QVERIFY(!plan.generatedProposals.isEmpty());
    QVERIFY(hasCapability(plan.generatedProposals, QStringLiteral("document_follow_up")));
}

QTEST_APPLESS_MAIN(SuggestionProposalBuilderMetadataTests)
#include "SuggestionProposalBuilderMetadataTests.moc"
