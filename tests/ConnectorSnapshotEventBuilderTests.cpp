#include <QtTest>

#include "connectors/ConnectorSnapshotEventBuilder.h"

class ConnectorSnapshotEventBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsInboxSnapshotEvent();
    void buildsResearchSnapshotEvent();
    void rejectsUnknownSnapshot();
};

void ConnectorSnapshotEventBuilderTests::buildsInboxSnapshotEvent()
{
    const QByteArray content = R"({
        "unreadCount": 3,
        "primarySender": "GitHub",
        "messages": [{"subject": "PR update"}]
    })";

    const ConnectorEvent event = ConnectorSnapshotEventBuilder::fromSnapshot(
        QStringLiteral("inbox"),
        QStringLiteral("D:/tmp/inbox.json"),
        content,
        QDateTime::fromString(QStringLiteral("2026-04-14T18:00:00.000Z"), Qt::ISODateWithMs),
        QStringLiteral("high"));
    QVERIFY(event.isValid());
    QCOMPARE(event.sourceKind, QStringLiteral("connector_snapshot"));
    QCOMPARE(event.connectorKind, QStringLiteral("inbox"));
    QCOMPARE(event.taskType, QStringLiteral("email_fetch"));
    QCOMPARE(event.summary, QStringLiteral("Inbox updated: GitHub needs attention."));
    QCOMPARE(event.priority, QStringLiteral("high"));
}

void ConnectorSnapshotEventBuilderTests::buildsResearchSnapshotEvent()
{
    const QByteArray content = R"({
        "query": "OpenAI release updates",
        "sources": [{"url": "https://example.com"}]
    })";

    const ConnectorEvent event = ConnectorSnapshotEventBuilder::fromSnapshot(
        QStringLiteral("research"),
        QStringLiteral("D:/tmp/research.json"),
        content,
        QDateTime::currentDateTimeUtc(),
        QStringLiteral("medium"));
    QVERIFY(event.isValid());
    QCOMPARE(event.connectorKind, QStringLiteral("research"));
    QCOMPARE(event.taskType, QStringLiteral("web_search"));
    QCOMPARE(event.sourceKind, QStringLiteral("connector_snapshot"));
}

void ConnectorSnapshotEventBuilderTests::rejectsUnknownSnapshot()
{
    const QByteArray content = R"({"items": []})";
    const ConnectorEvent event = ConnectorSnapshotEventBuilder::fromSnapshot(
        QStringLiteral("D:/tmp/unknown.json"),
        content,
        QDateTime::currentDateTimeUtc());
    QVERIFY(!event.isValid());
}

QTEST_APPLESS_MAIN(ConnectorSnapshotEventBuilderTests)
#include "ConnectorSnapshotEventBuilderTests.moc"
