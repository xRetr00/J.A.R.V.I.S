#include <QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "telemetry/BehavioralEventLedger.h"

class BehavioralEventLedgerTests : public QObject
{
    Q_OBJECT

private slots:
    void writesNdjsonArtifactsWhenSqliteDisabled();
    void returnsNoRecentEventsWhenSqliteIsDisabled();
};

void BehavioralEventLedgerTests::writesNdjsonArtifactsWhenSqliteDisabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    BehavioralEventLedger ledger(dir.path(), false);
    QVERIFY(ledger.initialize());

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("focus_mode"),
        QStringLiteral("state_transition"),
        QStringLiteral("focus_mode.enabled"),
        {
            { QStringLiteral("enabled"), true },
            { QStringLiteral("durationMinutes"), 60 }
        });
    event.sessionId = QStringLiteral("session-a");
    event.traceId = QStringLiteral("trace-a");
    event.threadId = QStringLiteral("thread-a");

    QVERIFY(ledger.recordEvent(event));
    QVERIFY(QFileInfo::exists(ledger.ndjsonPath()));
    QVERIFY(QFileInfo::exists(ledger.databasePath()));
}

void BehavioralEventLedgerTests::returnsNoRecentEventsWhenSqliteIsDisabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    BehavioralEventLedger ledger(dir.path(), false);
    QVERIFY(ledger.initialize());

    const QList<BehaviorTraceEvent> events = ledger.recentEvents(10);
    QVERIFY(events.isEmpty());
}

QTEST_APPLESS_MAIN(BehavioralEventLedgerTests)
#include "BehavioralEventLedgerTests.moc"
