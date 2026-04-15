#include <QtTest>

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "connectors/CalendarIcsEventBuilder.h"

class CalendarIcsEventBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsUpcomingScheduleEventFromIcsFile();
    void rejectsIcsFileWithoutEvents();
};

void CalendarIcsEventBuilderTests::buildsUpcomingScheduleEventFromIcsFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/today.ics");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "BEGIN:VCALENDAR\n";
    stream << "VERSION:2.0\n";
    stream << "BEGIN:VEVENT\n";
    stream << "SUMMARY:Sprint planning\n";
    stream << "DTSTART:20991201T090000Z\n";
    stream << "LOCATION:Discord\n";
    stream << "END:VEVENT\n";
    stream << "END:VCALENDAR\n";
    file.close();

    const ConnectorEvent event = CalendarIcsEventBuilder::fromFile(
        path,
        QFileInfo(path).lastModified().toUTC(),
        QStringLiteral("high"));
    QVERIFY(event.isValid());
    QCOMPARE(event.sourceKind, QStringLiteral("connector_schedule_calendar"));
    QCOMPARE(event.connectorKind, QStringLiteral("schedule"));
    QCOMPARE(event.taskType, QStringLiteral("calendar_review"));
    QCOMPARE(event.summary, QStringLiteral("Schedule updated: Sprint planning"));
    QCOMPARE(event.priority, QStringLiteral("high"));
    QCOMPARE(event.metadata.value(QStringLiteral("eventLocation")).toString(), QStringLiteral("Discord"));
}

void CalendarIcsEventBuilderTests::rejectsIcsFileWithoutEvents()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/empty.ics");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "BEGIN:VCALENDAR\nEND:VCALENDAR\n";
    file.close();

    const ConnectorEvent event = CalendarIcsEventBuilder::fromFile(
        path,
        QFileInfo(path).lastModified().toUTC());
    QVERIFY(!event.isValid());
}

QTEST_APPLESS_MAIN(CalendarIcsEventBuilderTests)
#include "CalendarIcsEventBuilderTests.moc"
