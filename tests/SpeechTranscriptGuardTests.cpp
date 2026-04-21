#include <QtTest>

#include "core/SpeechTranscriptGuard.h"

class SpeechTranscriptGuardTests : public QObject
{
    Q_OBJECT

private slots:
    void emptyTranscriptIgnored();
    void knownNonSpeechTokenIgnored();
    void knownArtifactIgnored();
    void correctionPhrasesNotIgnoredAsArtifact();
    void ambiguousShortTokenIgnored();
    void wakePhraseNotAmbiguous();
    void stopPhraseNotAmbiguous();
    void allowedSingleWordCommandAccepted();
    void lowSignalFollowUpIgnoredInActiveSession();
    void lowSignalFollowUpAcceptedOutsideActiveSession();
};

void SpeechTranscriptGuardTests::emptyTranscriptIgnored()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;

    const SpeechTranscriptDecision decision = guard.evaluate(QString(), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::IgnoreNonSpeech);
}

void SpeechTranscriptGuardTests::knownNonSpeechTokenIgnored()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("[applause]"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::IgnoreNonSpeech);
}

void SpeechTranscriptGuardTests::knownArtifactIgnored()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("Thanks for watching"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::IgnoreSttArtifact);
}

void SpeechTranscriptGuardTests::correctionPhrasesNotIgnoredAsArtifact()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;
    context.conversationSessionActive = true;

    const SpeechTranscriptDecision decision1 = guard.evaluate(QStringLiteral("I mean open AI."), context);
    QCOMPARE(decision1.disposition, SpeechTranscriptDisposition::Accept);

    const SpeechTranscriptDecision decision2 = guard.evaluate(QStringLiteral("I meant open AI."), context);
    QCOMPARE(decision2.disposition, SpeechTranscriptDisposition::Accept);

    const SpeechTranscriptDecision decision3 = guard.evaluate(QStringLiteral("No, I told you. I modeled by Oben AI."), context);
    QCOMPARE(decision3.disposition, SpeechTranscriptDisposition::Accept);
}

void SpeechTranscriptGuardTests::ambiguousShortTokenIgnored()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("you"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::IgnoreAmbiguous);
}

void SpeechTranscriptGuardTests::wakePhraseNotAmbiguous()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("hey vaxil"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::Accept);
    QVERIFY(decision.wakePhraseDetected);
}

void SpeechTranscriptGuardTests::stopPhraseNotAmbiguous()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("thanks"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::Accept);
    QVERIFY(decision.stopPhraseDetected);
}

void SpeechTranscriptGuardTests::allowedSingleWordCommandAccepted()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("open"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::Accept);
}

void SpeechTranscriptGuardTests::lowSignalFollowUpIgnoredInActiveSession()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;
    context.conversationSessionActive = true;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("you there"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::IgnoreAmbiguous);
}

void SpeechTranscriptGuardTests::lowSignalFollowUpAcceptedOutsideActiveSession()
{
    SpeechTranscriptGuard guard;
    SpeechTranscriptGuardContext context;
    context.conversationSessionActive = false;

    const SpeechTranscriptDecision decision = guard.evaluate(QStringLiteral("you there"), context);

    QCOMPARE(decision.disposition, SpeechTranscriptDisposition::Accept);
}

QTEST_APPLESS_MAIN(SpeechTranscriptGuardTests)
#include "SpeechTranscriptGuardTests.moc"
