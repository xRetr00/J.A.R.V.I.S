#pragma once

#include "core/TurnOrchestrationTypes.h"

class AssistantBehaviorPolicy;

class TurnOrchestrationRuntime
{
public:
    explicit TurnOrchestrationRuntime(const AssistantBehaviorPolicy *behaviorPolicy = nullptr);

    [[nodiscard]] TurnRuntimePlan buildPlan(const TurnRuntimeInput &input) const;

private:
    const AssistantBehaviorPolicy *m_behaviorPolicy = nullptr;
};
