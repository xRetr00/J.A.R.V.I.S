#include <QtTest>

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "connectors/InboxMaildropEventBuilder.h"

class InboxMaildropEventBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsInboxEventFromMaildropFile();
    void rejectsMaildropFileWithoutUsefulContent();
};

void InboxMaildropEventBuilderTests::buildsInboxEventFromMaildropFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/message.eml");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "From: GitHub\n";
    stream << "Subject: PR review requested\n";
    stream << "\n";
    stream << "Please review the latest patch.\n";
    file.close();

    const ConnectorEvent event = InboxMaildropEventBuilder::fromFile(
        path,
        QFileInfo(path).lastModified().toUTC());
    QVERIFY(event.isValid());
    QCOMPARE(event.sourceKind, QStringLiteral("connector_inbox_maildrop"));
    QCOMPARE(event.connectorKind, QStringLiteral("inbox"));
    QCOMPARE(event.taskType, QStringLiteral("email_fetch"));
    QCOMPARE(event.summary, QStringLiteral("Inbox updated: PR review requested from GitHub"));
    QCOMPARE(event.priority, QStringLiteral("high"));
}

void InboxMaildropEventBuilderTests::rejectsMaildropFileWithoutUsefulContent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/empty.eml");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "\n\n";
    file.close();

    const ConnectorEvent event = InboxMaildropEventBuilder::fromFile(
        path,
        QFileInfo(path).lastModified().toUTC());
    QVERIFY(!event.isValid());
}

QTEST_APPLESS_MAIN(InboxMaildropEventBuilderTests)
#include "InboxMaildropEventBuilderTests.moc"
