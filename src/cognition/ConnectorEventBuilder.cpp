#include "cognition/ConnectorEventBuilder.h"

#include <QUuid>

#include "cognition/ConnectorResultSignal.h"

namespace {
BackgroundTaskResult asBackgroundTaskResult(const AgentTask &task,
                                            const ToolExecutionResult &result)
{
    BackgroundTaskResult backgroundResult;
    backgroundResult.taskId = task.id;
    backgroundResult.type = task.type;
    backgroundResult.success = result.success;
    backgroundResult.errorKind = result.errorKind;
    backgroundResult.title = result.toolName;
    backgroundResult.summary = result.summary;
    backgroundResult.detail = result.detail;
    backgroundResult.payload = result.payload;
    backgroundResult.taskKey = task.taskKey;
    return backgroundResult;
}
}

ConnectorEvent ConnectorEventBuilder::fromBackgroundTaskResult(const BackgroundTaskResult &result)
{
    const ConnectorResultSignal signal = ConnectorResultSignalBuilder::fromBackgroundTaskResult(result);
    if (!signal.isValid()) {
        return {};
    }

    ConnectorEvent event;
    event.eventId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.sourceKind = signal.sourceKind;
    event.connectorKind = signal.connectorKind;
    event.taskType = signal.taskType;
    event.summary = signal.summary;
    event.taskKey = result.taskKey;
    event.taskId = result.taskId;
    event.itemCount = signal.itemCount;
    event.priority = result.success ? QStringLiteral("medium") : QStringLiteral("high");
    event.metadata = {
        {QStringLiteral("resultType"), result.type},
        {QStringLiteral("resultSuccess"), result.success},
        {QStringLiteral("resultSummary"), result.summary},
        {QStringLiteral("resultTitle"), result.title}
    };
    return event;
}

ConnectorEvent ConnectorEventBuilder::fromTaskExecution(const AgentTask &task,
                                                        const ToolExecutionResult &result)
{
    const BackgroundTaskResult backgroundResult = asBackgroundTaskResult(task, result);
    ConnectorEvent event = fromBackgroundTaskResult(backgroundResult);
    if (!event.isValid()) {
        return {};
    }

    event.sourceKind = QStringLiteral("connector_live");
    event.metadata.insert(QStringLiteral("producer"), QStringLiteral("tool_worker"));
    event.metadata.insert(QStringLiteral("toolName"), result.toolName);
    event.metadata.insert(QStringLiteral("taskTypeRaw"), task.type);
    return event;
}
