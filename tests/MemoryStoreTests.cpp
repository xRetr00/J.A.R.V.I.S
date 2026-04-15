#include <QtTest>

#include <QTemporaryDir>

#include "memory/MemoryStore.h"

class MemoryStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void returnsConnectorMemoryRecordsForRelevantQuery();
    void persistsCompiledContextPolicyMemory();
    void persistsCompiledContextPolicyHistory();
    void persistsAndRollsBackCompiledContextPolicyTuningState();
};

void MemoryStoreTests::returnsConnectorMemoryRecordsForRelevantQuery()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MemoryStore store(dir.path() + QStringLiteral("/memory.json"));

    QVERIFY(store.upsertConnectorState(QStringLiteral("connector:schedule:today.ics"), {
        {QStringLiteral("sourceKind"), QStringLiteral("connector_schedule_calendar")},
        {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
        {QStringLiteral("seenCount"), 3},
        {QStringLiteral("presentedCount"), 1},
        {QStringLiteral("historyRecentlySeen"), true},
        {QStringLiteral("historyRecentlyPresented"), false},
        {QStringLiteral("lastSeenAtMs"), 1200}
    }));

    const QList<MemoryRecord> records = store.connectorMemory(QStringLiteral("schedule"));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().source, QStringLiteral("connector_memory"));
    QCOMPARE(records.first().key, QStringLiteral("connector_history_schedule"));
    QVERIFY(records.first().value.contains(QStringLiteral("Schedule signals seen 3 times")));
}

void MemoryStoreTests::persistsCompiledContextPolicyMemory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MemoryStore store(dir.path() + QStringLiteral("/memory.json"));

    QVERIFY(store.upsertCompiledContextPolicyState({
        {QStringLiteral("dominantMode"), QStringLiteral("document_work")},
        {QStringLiteral("selectionDirective"), QStringLiteral("History policy: stable document-focused work is ongoing.")},
        {QStringLiteral("promptDirective"), QStringLiteral("Stable mode: document-focused work remains active.")},
        {QStringLiteral("reasonCode"), QStringLiteral("compiled_history_policy.document_work")},
        {QStringLiteral("strength"), 2.8},
        {QStringLiteral("updatedAtMs"), 4200}
    }));

    const QList<MemoryRecord> records = store.compiledContextPolicyMemory(QStringLiteral("document"));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().source, QStringLiteral("compiled_history_policy_memory"));
    QCOMPARE(records.first().key, QStringLiteral("compiled_context_history_mode"));
    QVERIFY(records.first().value.contains(QStringLiteral("document_work")));
}

void MemoryStoreTests::persistsCompiledContextPolicyHistory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MemoryStore store(dir.path() + QStringLiteral("/memory.json"));

    QVERIFY(store.upsertCompiledContextPolicyState({
        {QStringLiteral("dominantMode"), QStringLiteral("document_work")},
        {QStringLiteral("selectionDirective"), QStringLiteral("History policy: stable document-focused work is ongoing.")},
        {QStringLiteral("promptDirective"), QStringLiteral("Stable mode: document-focused work remains active.")},
        {QStringLiteral("reasonCode"), QStringLiteral("compiled_history_policy.document_work")},
        {QStringLiteral("strength"), 2.4},
        {QStringLiteral("updatedAtMs"), 4200}
    }));
    QVERIFY(store.upsertCompiledContextPolicyState({
        {QStringLiteral("dominantMode"), QStringLiteral("research_analysis")},
        {QStringLiteral("selectionDirective"), QStringLiteral("History policy: stable research analysis is ongoing.")},
        {QStringLiteral("promptDirective"), QStringLiteral("Stable mode: research analysis remains active.")},
        {QStringLiteral("reasonCode"), QStringLiteral("compiled_history_policy.research_analysis")},
        {QStringLiteral("strength"), 2.8},
        {QStringLiteral("updatedAtMs"), 5200}
    }));

    const QVariantList history = store.compiledContextPolicyHistory();
    QCOMPARE(history.size(), 2);
    QCOMPARE(history.first().toMap().value(QStringLiteral("dominantMode")).toString(),
             QStringLiteral("document_work"));
    QCOMPARE(history.last().toMap().value(QStringLiteral("dominantMode")).toString(),
             QStringLiteral("research_analysis"));
}

void MemoryStoreTests::persistsAndRollsBackCompiledContextPolicyTuningState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MemoryStore store(dir.path() + QStringLiteral("/memory.json"));

    QVERIFY(store.promoteCompiledContextPolicyTuningState({
        {QStringLiteral("tuningCurrentMode"), QStringLiteral("document_work")},
        {QStringLiteral("tuningVolatilityLevel"), QStringLiteral("steady")},
        {QStringLiteral("tuningAlignmentBoost"), 0.08},
        {QStringLiteral("tuningDefocusPenalty"), 0.07},
        {QStringLiteral("tuningVolatilityPenalty"), 0.05},
        {QStringLiteral("tuningSuppressionScoreThreshold"), 0.72},
        {QStringLiteral("updatedAtMs"), 4200}
    }));
    QVERIFY(store.promoteCompiledContextPolicyTuningState({
        {QStringLiteral("tuningCurrentMode"), QStringLiteral("research_analysis")},
        {QStringLiteral("tuningVolatilityLevel"), QStringLiteral("elevated")},
        {QStringLiteral("tuningAlignmentBoost"), 0.10},
        {QStringLiteral("tuningDefocusPenalty"), 0.08},
        {QStringLiteral("tuningVolatilityPenalty"), 0.08},
        {QStringLiteral("tuningSuppressionScoreThreshold"), 0.78},
        {QStringLiteral("updatedAtMs"), 5200}
    }));

    QCOMPARE(store.compiledContextPolicyTuningState().value(QStringLiteral("tuningCurrentMode")).toString(),
             QStringLiteral("research_analysis"));
    QCOMPARE(store.compiledContextPolicyTuningState().value(QStringLiteral("version")).toInt(), 2);
    QCOMPARE(store.compiledContextPolicyTuningHistory().size(), 2);

    QVERIFY(store.rollbackCompiledContextPolicyTuningState({
        {QStringLiteral("tuningPromotionAction"), QStringLiteral("rollback")},
        {QStringLiteral("tuningPromotionReason"), QStringLiteral("behavior_tuning.rollback_test")}
    }));
    QCOMPARE(store.compiledContextPolicyTuningState().value(QStringLiteral("tuningCurrentMode")).toString(),
             QStringLiteral("document_work"));
    QCOMPARE(store.compiledContextPolicyTuningState().value(QStringLiteral("tuningPromotionAction")).toString(),
             QStringLiteral("rollback"));
    QCOMPARE(store.compiledContextPolicyTuningHistory().size(), 1);
}

QTEST_APPLESS_MAIN(MemoryStoreTests)
#include "MemoryStoreTests.moc"
