#include <QtTest>

#include "cognition/SuggestionProposalRanker.h"

class SuggestionProposalRankerWorkModeTests : public QObject
{
    Q_OBJECT

private slots:
    void boostsResearchProposalInBrowserResearchContext();
};

void SuggestionProposalRankerWorkModeTests::boostsResearchProposalInBrowserResearchContext()
{
    const QList<RankedSuggestionProposal> ranked = SuggestionProposalRanker::rank({
        .proposals = {
            ActionProposal{.proposalId = QStringLiteral("p1"),
                           .capabilityId = QStringLiteral("source_review"),
                           .title = QStringLiteral("Review docs"),
                           .summary = QStringLiteral("I can review the current documentation sources."),
                           .priority = QStringLiteral("medium")},
            ActionProposal{.proposalId = QStringLiteral("p2"),
                           .capabilityId = QStringLiteral("schedule_follow_up"),
                           .title = QStringLiteral("Review schedule"),
                           .summary = QStringLiteral("I can prepare your upcoming schedule."),
                           .priority = QStringLiteral("medium")}
        },
        .desktopContext = {
            {QStringLiteral("taskId"), QStringLiteral("browser_tab")},
            {QStringLiteral("metadataClass"), QStringLiteral("browser")},
            {QStringLiteral("siteContext"), QStringLiteral("docs.qt.io")}
        },
        .desktopContextAtMs = 1000,
        .cooldownState = CooldownState{},
        .focusMode = FocusModeState{},
        .nowMs = 1500
    });

    QVERIFY(!ranked.isEmpty());
    QCOMPARE(ranked.first().proposal.capabilityId, QStringLiteral("source_review"));
    QCOMPARE(ranked.first().reasonCode, QStringLiteral("proposal_rank.desktop_research_mode"));
}

QTEST_APPLESS_MAIN(SuggestionProposalRankerWorkModeTests)
#include "SuggestionProposalRankerWorkModeTests.moc"
