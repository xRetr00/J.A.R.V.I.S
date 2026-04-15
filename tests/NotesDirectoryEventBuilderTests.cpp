#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "connectors/NotesDirectoryEventBuilder.h"

class NotesDirectoryEventBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsMarkdownNoteEvent();
    void ignoresUnsupportedFile();
};

void NotesDirectoryEventBuilderTests::buildsMarkdownNoteEvent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/today.md");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "# Daily plan\nship connector work\n";
    file.close();

    const ConnectorEvent event = NotesDirectoryEventBuilder::fromFile(
        path,
        QFileInfo(path).lastModified().toUTC());
    QVERIFY(event.isValid());
    QCOMPARE(event.sourceKind, QStringLiteral("connector_notes_directory"));
    QCOMPARE(event.connectorKind, QStringLiteral("notes"));
    QCOMPARE(event.taskType, QStringLiteral("note_review"));
    QCOMPARE(event.summary, QStringLiteral("Notes updated: Daily plan"));
}

void NotesDirectoryEventBuilderTests::ignoresUnsupportedFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/blob.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "{\"title\":\"ignored\"}";
    file.close();

    const ConnectorEvent event = NotesDirectoryEventBuilder::fromFile(
        path,
        QFileInfo(path).lastModified().toUTC());
    QVERIFY(!event.isValid());
}

QTEST_APPLESS_MAIN(NotesDirectoryEventBuilderTests)
#include "NotesDirectoryEventBuilderTests.moc"
