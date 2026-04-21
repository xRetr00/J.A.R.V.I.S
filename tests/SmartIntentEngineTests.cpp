#include <QtTest>

#include "core/intent/TurnSignalExtractor.h"

class SmartIntentEngineTests : public QObject
{
    Q_OBJECT

private slots:
    void marksPureGreetingAsSocialOnly();
    void marksPureSmallTalkAsSocialOnly();
    void keepsGreetingPlusRequestOutOfSocialOnly();
    void keepsSmallTalkPlusQuestionOutOfSocialOnly();
    void doesNotTreatOpenSourceAsCommandCue();
    void detectsStructuredCommandWithSocialPrefix();
    void marksInfoQuestionWithoutCommandCue();
};

void SmartIntentEngineTests::marksPureGreetingAsSocialOnly()
{
    TurnSignalExtractor extractor;
    const TurnSignals extracted = extractor.extract(QStringLiteral("hello"));
    QVERIFY(extracted.hasGreeting);
    QVERIFY(extracted.socialOnly);
}

void SmartIntentEngineTests::marksPureSmallTalkAsSocialOnly()
{
    TurnSignalExtractor extractor;
    const TurnSignals extracted = extractor.extract(QStringLiteral("thanks"));
    QVERIFY(extracted.hasSmallTalk);
    QVERIFY(extracted.socialOnly);
}

void SmartIntentEngineTests::keepsGreetingPlusRequestOutOfSocialOnly()
{
    TurnSignalExtractor extractor;
    const TurnSignals extracted = extractor.extract(QStringLiteral("hi explain this error"));
    QVERIFY(extracted.hasGreeting);
    QVERIFY(extracted.hasQuestionCue);
    QVERIFY(!extracted.socialOnly);
}

void SmartIntentEngineTests::keepsSmallTalkPlusQuestionOutOfSocialOnly()
{
    TurnSignalExtractor extractor;
    const TurnSignals extracted = extractor.extract(QStringLiteral("thanks, why is this slow?"));
    QVERIFY(extracted.hasSmallTalk);
    QVERIFY(extracted.hasQuestionCue);
    QVERIFY(!extracted.socialOnly);
}

void SmartIntentEngineTests::doesNotTreatOpenSourceAsCommandCue()
{
    TurnSignalExtractor extractor;
    const TurnSignals extracted = extractor.extract(QStringLiteral("explain open source licensing"));
    QVERIFY(!extracted.hasCommandCue);
}

void SmartIntentEngineTests::detectsStructuredCommandWithSocialPrefix()
{
    TurnSignalExtractor extractor;
    const TurnSignals extracted = extractor.extract(QStringLiteral("hey open youtube"));
    QVERIFY(extracted.hasGreeting);
    QVERIFY(extracted.hasCommandCue);
}

void SmartIntentEngineTests::marksInfoQuestionWithoutCommandCue()
{
    TurnSignalExtractor extractor;
    const TurnSignals extracted = extractor.extract(QStringLiteral("what does open source mean?"));
    QVERIFY(extracted.hasQuestionCue);
    QVERIFY(!extracted.hasCommandCue);
}

QTEST_APPLESS_MAIN(SmartIntentEngineTests)
#include "SmartIntentEngineTests.moc"
