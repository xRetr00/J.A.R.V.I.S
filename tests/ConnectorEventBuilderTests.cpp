#include <QtTest>

#include "cognition/ConnectorEventBuilder.h"

class ConnectorEventBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsScheduleConnectorEvent();
    void buildsLiveResearchConnectorEvent();
    void rejectsUnknownPayload();
};

void ConnectorEventBuilderTests::buildsScheduleConnectorEvent()
{
    BackgroundTaskResult result;
    result.taskId = 7;
    result.taskKey = QStringLiteral("calendar-sync");
    result.type = QStringLiteral("background_sync");
    result.success = true;
    result.summary = QStringLiteral("Calendar sync complete.");
    result.title = QStringLiteral("Calendar");
    result.payload = QJsonObject{
        {QStringLiteral("connectorKind"), QStringLiteral("schedule")},
        {QStringLiteral("events"), QJsonArray{
             QJsonObject{{QStringLiteral("title"), QStringLiteral("Design review")}}
         }},
        {QStringLiteral("title"), QStringLiteral("Design review")}
    };

    const ConnectorEvent event = ConnectorEventBuilder::fromBackgroundTaskResult(result);
    QVERIFY(event.isValid());
    QVERIFY(!event.eventId.trimmed().isEmpty());
    QCOMPARE(event.sourceKind, QStringLiteral("connector_result"));
    QCOMPARE(event.connectorKind, QStringLiteral("schedule"));
    QCOMPARE(event.taskType, QStringLiteral("calendar_review"));
    QCOMPARE(event.taskKey, QStringLiteral("calendar-sync"));
    QCOMPARE(event.taskId, 7);
    QCOMPARE(event.priority, QStringLiteral("medium"));
    QCOMPARE(event.metadata.value(QStringLiteral("resultType")).toString(), QStringLiteral("background_sync"));
    QCOMPARE(event.metadata.value(QStringLiteral("resultSuccess")).toBool(), true);
}

void ConnectorEventBuilderTests::rejectsUnknownPayload()
{
    BackgroundTaskResult result;
    result.type = QStringLiteral("background_sync");
    result.summary = QStringLiteral("Nothing relevant.");
    result.payload = QJsonObject{
        {QStringLiteral("items"), QJsonArray{}}
    };

    const ConnectorEvent event = ConnectorEventBuilder::fromBackgroundTaskResult(result);
    QVERIFY(!event.isValid());
}

void ConnectorEventBuilderTests::buildsLiveResearchConnectorEvent()
{
    AgentTask task;
    task.id = 14;
    task.type = QStringLiteral("background_sync");
    task.taskKey = QStringLiteral("research:openai");

    ToolExecutionResult result;
    result.toolName = QStringLiteral("web_search");
    result.success = true;
    result.summary = QStringLiteral("Research updated.");
    result.payload = QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("brave")},
        {QStringLiteral("query"), QStringLiteral("OpenAI release updates")},
        {QStringLiteral("sources"), QJsonArray{
             QJsonObject{{QStringLiteral("url"), QStringLiteral("https://example.com")}}
         }}
    };

    const ConnectorEvent event = ConnectorEventBuilder::fromTaskExecution(task, result);
    QVERIFY(event.isValid());
    QCOMPARE(event.sourceKind, QStringLiteral("connector_live"));
    QCOMPARE(event.connectorKind, QStringLiteral("research"));
    QCOMPARE(event.taskType, QStringLiteral("web_search"));
    QCOMPARE(event.taskKey, QStringLiteral("research:openai"));
    QCOMPARE(event.taskId, 14);
    QCOMPARE(event.metadata.value(QStringLiteral("producer")).toString(), QStringLiteral("tool_worker"));
    QCOMPARE(event.metadata.value(QStringLiteral("toolName")).toString(), QStringLiteral("web_search"));
}

QTEST_APPLESS_MAIN(ConnectorEventBuilderTests)
#include "ConnectorEventBuilderTests.moc"
