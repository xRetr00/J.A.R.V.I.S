#pragma once

#include "companion/contracts/ConnectorEvent.h"
#include "core/AssistantTypes.h"

class ConnectorEventBuilder
{
public:
    [[nodiscard]] static ConnectorEvent fromBackgroundTaskResult(const BackgroundTaskResult &result);
    [[nodiscard]] static ConnectorEvent fromTaskExecution(const AgentTask &task,
                                                         const ToolExecutionResult &result);
};
