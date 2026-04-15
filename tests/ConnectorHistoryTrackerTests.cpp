#include <QtTest>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "cognition/ConnectorHistoryTracker.h"
#include "memory/MemoryStore.h"

class ConnectorHistoryTrackerTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsPerKeyAndPerConnectorHistoryMetadata();
    void persistsHistoryAcrossTrackerInstances();
};

void ConnectorHistoryTrackerTests::buildsPerKeyAndPerConnectorHistoryMetadata()
{
    ConnectorHistoryTracker tracker;
    tracker.recordSeen(QStringLiteral("connector_inbox_maildrop"),
                       QStringLiteral("inbox"),
                       QStringLiteral("connector:inbox:message-1"),
                       1000);
    tracker.recordSeen(QStringLiteral("connector_inbox_maildrop"),
                       QStringLiteral("inbox"),
                       QStringLiteral("connector:inbox:message-2"),
                       1500);
    tracker.recordPresented(QStringLiteral("connector:inbox:message-1"), 1800);

    const QVariantMap metadata = tracker.buildMetadata(
        QStringLiteral("connector_inbox_maildrop"),
        QStringLiteral("inbox"),
        QStringLiteral("connector:inbox:message-1"),
        2000);

    QCOMPARE(metadata.value(QStringLiteral("historySeenCount")).toInt(), 1);
    QCOMPARE(metadata.value(QStringLiteral("historyPresentedCount")).toInt(), 1);
    QCOMPARE(metadata.value(QStringLiteral("connectorKindRecentSeenCount")).toInt(), 2);
    QCOMPARE(metadata.value(QStringLiteral("connectorKindRecentPresentedCount")).toInt(), 1);
    QVERIFY(metadata.value(QStringLiteral("historyRecentlySeen")).toBool());
    QVERIFY(metadata.value(QStringLiteral("historyRecentlyPresented")).toBool());
}

void ConnectorHistoryTrackerTests::persistsHistoryAcrossTrackerInstances()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString storagePath = dir.path() + QStringLiteral("/memory.json");

    {
        MemoryStore store(storagePath);
        ConnectorHistoryTracker tracker(&store);
        tracker.recordSeen(QStringLiteral("connector_schedule_calendar"),
                           QStringLiteral("schedule"),
                           QStringLiteral("connector:schedule:today.ics"),
                           1000);
        tracker.recordPresented(QStringLiteral("connector:schedule:today.ics"), 1200);
    }

    MemoryStore reloadedStore(storagePath);
    ConnectorHistoryTracker reloaded(&reloadedStore);
    const QVariantMap metadata = reloaded.buildMetadata(
        QStringLiteral("connector_schedule_calendar"),
        QStringLiteral("schedule"),
        QStringLiteral("connector:schedule:today.ics"),
        1500);

    QCOMPARE(metadata.value(QStringLiteral("historySeenCount")).toInt(), 1);
    QCOMPARE(metadata.value(QStringLiteral("historyPresentedCount")).toInt(), 1);
    QCOMPARE(metadata.value(QStringLiteral("historyFirstSeenAtMs")).toLongLong(), 1000);
    QCOMPARE(metadata.value(QStringLiteral("historyLastSeenAtMs")).toLongLong(), 1000);
    QCOMPARE(metadata.value(QStringLiteral("historyLastPresentedAtMs")).toLongLong(), 1200);
    QVERIFY(QFileInfo::exists(storagePath));
}

QTEST_APPLESS_MAIN(ConnectorHistoryTrackerTests)
#include "ConnectorHistoryTrackerTests.moc"
