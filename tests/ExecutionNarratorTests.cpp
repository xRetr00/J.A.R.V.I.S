#include <QtTest>

#include "core/ExecutionNarrator.h"

class ExecutionNarratorTests : public QObject
{
    Q_OBJECT

private slots:
    void suppressesInternalPlanningHintsInClarifications();
    void suppressesInternalPlanningHintsInPreActionText();
    void preservesUserFacingHints();
};

void ExecutionNarratorTests::suppressesInternalPlanningHintsInClarifications()
{
    ExecutionNarrator narrator;
    ActionSession session;
    session.responseMode = ResponseMode::Clarify;
    session.nextStepHint = QStringLiteral("Ground the answer in retrieved evidence before responding.");

    const QString text = narrator.clarificationPrompt(session);
    QCOMPARE(text, QStringLiteral("I didn't catch that."));
}

void ExecutionNarratorTests::suppressesInternalPlanningHintsInPreActionText()
{
    ExecutionNarrator narrator;
    ActionSession session;
    session.responseMode = ResponseMode::ActWithProgress;
    session.shouldAnnounceProgress = true;
    session.progress = QStringLiteral("Working through it now.");
    session.nextStepHint = QStringLiteral("Inspect and verify state before taking any side-effecting action.");

    const QString text = narrator.preActionText(session, QStringLiteral("Fallback"));
    QCOMPARE(text, QStringLiteral("Working through it now."));
}

void ExecutionNarratorTests::preservesUserFacingHints()
{
    ExecutionNarrator narrator;
    ActionSession session;
    session.responseMode = ResponseMode::Recover;
    session.nextStepHint = QStringLiteral("If you want, I can try a narrower search.");

    const QString text = narrator.outcomeSummary(session, false, QStringLiteral("I couldn't verify that yet."));
    QVERIFY(text.contains(QStringLiteral("I couldn't verify that yet.")));
    QVERIFY(text.contains(QStringLiteral("If you want, I can try a narrower search.")));
}

QTEST_APPLESS_MAIN(ExecutionNarratorTests)
#include "ExecutionNarratorTests.moc"
