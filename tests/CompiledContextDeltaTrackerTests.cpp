#include <QtTest>

#include "cognition/CompiledContextDeltaTracker.h"

class CompiledContextDeltaTrackerTests : public QObject
{
    Q_OBJECT

private slots:
    void detectsAddedRemovedKeysAndSummaryChanges();
};

void CompiledContextDeltaTrackerTests::detectsAddedRemovedKeysAndSummaryChanges()
{
    const CompiledContextDelta delta = CompiledContextDeltaTracker::evaluate(
        QStringLiteral("Desktop context: browser tab."),
        {QStringLiteral("desktop_context_summary"), QStringLiteral("desktop_context_task")},
        QStringLiteral("Desktop context: browser tab. Site: github.com."),
        {QStringLiteral("desktop_context_summary"), QStringLiteral("desktop_context_task"), QStringLiteral("desktop_context_site")});

    QVERIFY(delta.hasChanges());
    QVERIFY(delta.summaryChanged);
    QCOMPARE(delta.addedKeys.size(), 1);
    QCOMPARE(delta.addedKeys.first(), QStringLiteral("desktop_context_site"));
    QVERIFY(delta.removedKeys.isEmpty());
}

QTEST_APPLESS_MAIN(CompiledContextDeltaTrackerTests)
#include "CompiledContextDeltaTrackerTests.moc"
