#include <QtTest>

#include "ai/ReasoningRouter.h"

class ReasoningRouterTests : public QObject
{
    Q_OBJECT

private slots:
    void routesCommandsToFast();
    void routesAnalysisToDeep();
    void routesShortCasualToFast();
    void respectsDefaultModeWhenAutoRoutingDisabled();
};

void ReasoningRouterTests::routesCommandsToFast()
{
    ReasoningRouter router;
    QCOMPARE(router.chooseMode(QStringLiteral("turn off the light"), true, ReasoningMode::Balanced), ReasoningMode::Fast);
}

void ReasoningRouterTests::routesAnalysisToDeep()
{
    ReasoningRouter router;
    QCOMPARE(router.chooseMode(QStringLiteral("explain the strategy behind this"), true, ReasoningMode::Balanced), ReasoningMode::Deep);
}

void ReasoningRouterTests::routesShortCasualToFast()
{
    ReasoningRouter router;
    QCOMPARE(router.chooseMode(QStringLiteral("what time"), true, ReasoningMode::Balanced), ReasoningMode::Fast);
}

void ReasoningRouterTests::respectsDefaultModeWhenAutoRoutingDisabled()
{
    ReasoningRouter router;
    QCOMPARE(router.chooseMode(QStringLiteral("turn off the light"), false, ReasoningMode::Deep), ReasoningMode::Deep);
}

QTEST_APPLESS_MAIN(ReasoningRouterTests)
#include "ReasoningRouterTests.moc"
