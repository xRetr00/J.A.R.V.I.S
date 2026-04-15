#include <QtTest>
#include <QDateTime>

#include "cognition/ConnectorContextCompiler.h"

class ConnectorContextCompilerTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsLongHorizonConnectorSummary();
    void ranksRelevantConnectorSummaryForQuery();
    void prefersHigherPriorityFresherConnectorSummary();
};

void ConnectorContextCompilerTests::buildsLongHorizonConnectorSummary()
{
    const QHash<QString, QVariantMap> stateByKey = {
        {QStringLiteral("connector:inbox:a"),
         {
             {QStringLiteral("historyKey"), QStringLiteral("connector:inbox:a")},
             {QStringLiteral("connectorKind"), QStringLiteral("inbox")},
             {QStringLiteral("sourceKind"), QStringLiteral("connector_inbox_maildrop")},
             {QStringLiteral("firstSeenAtMs"), 1000},
             {QStringLiteral("seenCount"), 4},
             {QStringLiteral("presentedCount"), 1},
             {QStringLiteral("historyRecentlySeen"), true},
             {QStringLiteral("historyRecentlyPresented"), false},
             {QStringLiteral("lastPresentedAtMs"), 1900},
             {QStringLiteral("lastSeenAtMs"), 2000}
         }},
        {QStringLiteral("connector:inbox:b"),
         {
             {QStringLiteral("historyKey"), QStringLiteral("connector:inbox:b")},
             {QStringLiteral("connectorKind"), QStringLiteral("inbox")},
             {QStringLiteral("sourceKind"), QStringLiteral("connector_inbox_maildrop")},
             {QStringLiteral("firstSeenAtMs"), 1500},
             {QStringLiteral("seenCount"), 3},
             {QStringLiteral("presentedCount"), 2},
             {QStringLiteral("historyRecentlySeen"), true},
             {QStringLiteral("historyRecentlyPresented"), true},
             {QStringLiteral("lastPresentedAtMs"), 2800},
             {QStringLiteral("lastSeenAtMs"), 3000}
         }}
    };

    const QList<MemoryRecord> summaries = ConnectorContextCompiler::compileSummaries(QStringLiteral("inbox"), stateByKey);
    QCOMPARE(summaries.size(), 1);
    QCOMPARE(summaries.first().key, QStringLiteral("connector_summary_inbox"));
    QCOMPARE(summaries.first().source, QStringLiteral("connector_summary"));
    QVERIFY(summaries.first().value.contains(QStringLiteral("across 2 sources")));
    QVERIFY(summaries.first().value.contains(QStringLiteral("seen 7 times")));
    QVERIFY(summaries.first().value.contains(QStringLiteral("surfaced 3 times")));
    QVERIFY(summaries.first().value.contains(QStringLiteral("top source maildrop")));
}

void ConnectorContextCompilerTests::ranksRelevantConnectorSummaryForQuery()
{
    const QHash<QString, QVariantMap> stateByKey = {
        {QStringLiteral("connector:schedule:today"),
         {
             {QStringLiteral("historyKey"), QStringLiteral("connector:schedule:today")},
             {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
             {QStringLiteral("sourceKind"), QStringLiteral("connector_schedule_calendar")},
             {QStringLiteral("firstSeenAtMs"), 2000},
             {QStringLiteral("seenCount"), 2},
             {QStringLiteral("presentedCount"), 1},
             {QStringLiteral("historyRecentlySeen"), true},
             {QStringLiteral("historyRecentlyPresented"), false},
             {QStringLiteral("lastPresentedAtMs"), 3500},
             {QStringLiteral("lastSeenAtMs"), 4000}
         }},
        {QStringLiteral("connector:research:bookmarks"),
         {
             {QStringLiteral("historyKey"), QStringLiteral("connector:research:bookmarks")},
             {QStringLiteral("connectorKind"), QStringLiteral("research")},
             {QStringLiteral("sourceKind"), QStringLiteral("connector_research_browser")},
             {QStringLiteral("firstSeenAtMs"), 2500},
             {QStringLiteral("seenCount"), 5},
             {QStringLiteral("presentedCount"), 2},
             {QStringLiteral("historyRecentlySeen"), true},
             {QStringLiteral("historyRecentlyPresented"), true},
             {QStringLiteral("lastPresentedAtMs"), 4600},
             {QStringLiteral("lastSeenAtMs"), 5000}
         }}
    };

    const QList<MemoryRecord> summaries = ConnectorContextCompiler::compileSummaries(QStringLiteral("schedule"), stateByKey, 1);
    QCOMPARE(summaries.size(), 1);
    QCOMPARE(summaries.first().key, QStringLiteral("connector_summary_schedule"));
}

void ConnectorContextCompilerTests::prefersHigherPriorityFresherConnectorSummary()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QHash<QString, QVariantMap> stateByKey = {
        {QStringLiteral("connector:schedule:next"),
         {
             {QStringLiteral("historyKey"), QStringLiteral("connector:schedule:next")},
             {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
             {QStringLiteral("sourceKind"), QStringLiteral("connector_schedule_calendar")},
             {QStringLiteral("firstSeenAtMs"), nowMs - (2LL * 24LL * 60LL * 60LL * 1000LL)},
             {QStringLiteral("seenCount"), 2},
             {QStringLiteral("presentedCount"), 1},
             {QStringLiteral("historyRecentlySeen"), true},
             {QStringLiteral("historyRecentlyPresented"), false},
             {QStringLiteral("lastPresentedAtMs"), nowMs - (15LL * 60LL * 1000LL)},
             {QStringLiteral("lastSeenAtMs"), nowMs - (5LL * 60LL * 1000LL)}
         }},
        {QStringLiteral("connector:research:old"),
         {
             {QStringLiteral("historyKey"), QStringLiteral("connector:research:old")},
             {QStringLiteral("connectorKind"), QStringLiteral("research")},
             {QStringLiteral("sourceKind"), QStringLiteral("connector_research_browser")},
             {QStringLiteral("firstSeenAtMs"), nowMs - (10LL * 24LL * 60LL * 60LL * 1000LL)},
             {QStringLiteral("seenCount"), 6},
             {QStringLiteral("presentedCount"), 3},
             {QStringLiteral("historyRecentlySeen"), false},
             {QStringLiteral("historyRecentlyPresented"), false},
             {QStringLiteral("lastPresentedAtMs"), nowMs - (8LL * 24LL * 60LL * 60LL * 1000LL)},
             {QStringLiteral("lastSeenAtMs"), nowMs - (8LL * 24LL * 60LL * 60LL * 1000LL)}
         }}
    };

    const QList<MemoryRecord> summaries = ConnectorContextCompiler::compileSummaries(QString(), stateByKey, 1);
    QCOMPARE(summaries.size(), 1);
    QCOMPARE(summaries.first().key, QStringLiteral("connector_summary_schedule"));
    QVERIFY(summaries.first().value.contains(QStringLiteral("calendar")));
}

QTEST_APPLESS_MAIN(ConnectorContextCompilerTests)
#include "ConnectorContextCompilerTests.moc"
