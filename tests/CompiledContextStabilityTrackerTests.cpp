#include <QtTest>

#include "cognition/CompiledContextStabilityTracker.h"

class CompiledContextStabilityTrackerTests : public QObject
{
    Q_OBJECT

private slots:
    void reportsFreshContextWhenNotStable();
    void reportsStableContextSummary();
};

void CompiledContextStabilityTrackerTests::reportsFreshContextWhenNotStable()
{
    const CompiledContextStabilitySummary summary = CompiledContextStabilityTracker::evaluate(
        QStringLiteral("Desktop context: editing CooldownEngine.cpp."),
        {QStringLiteral("desktop_context_summary"),
         QStringLiteral("desktop_context_document")},
        0,
        0);

    QVERIFY(!summary.stableContext);
    QCOMPARE(summary.stableCycles, 0);
    QVERIFY(summary.summaryText.contains(QStringLiteral("fresh")));
}

void CompiledContextStabilityTrackerTests::reportsStableContextSummary()
{
    const CompiledContextStabilitySummary summary = CompiledContextStabilityTracker::evaluate(
        QStringLiteral("Desktop context: editing CooldownEngine.cpp. Document: CooldownEngine.cpp."),
        {QStringLiteral("desktop_context_summary"),
         QStringLiteral("desktop_context_document"),
         QStringLiteral("connector_summary_schedule")},
        3,
        90000);

    QVERIFY(summary.stableContext);
    QCOMPARE(summary.stableCycles, 3);
    QCOMPARE(summary.stableDurationMs, 90000);
    QVERIFY(summary.stableKeys.contains(QStringLiteral("desktop_context_summary")));
    QVERIFY(summary.stableKeys.contains(QStringLiteral("connector_summary_schedule")));
    QVERIFY(summary.summaryText.contains(QStringLiteral("Context stable for 3 cycles")));
}

QTEST_APPLESS_MAIN(CompiledContextStabilityTrackerTests)
#include "CompiledContextStabilityTrackerTests.moc"
