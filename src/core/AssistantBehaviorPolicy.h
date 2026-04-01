#pragma once

#include "core/AssistantTypes.h"

struct InputRouterContext;

class AssistantBehaviorPolicy
{
public:
    InputRouteDecision decideRoute(const InputRouterContext &context) const;
    MemoryContext buildMemoryContext(const QString &input, const QList<MemoryRecord> &memory) const;
    ToolPlan buildToolPlan(const QString &input,
                           IntentType intent,
                           const QList<AgentToolSpec> &availableTools) const;
    QList<AgentToolSpec> selectRelevantTools(const QString &input,
                                             IntentType intent,
                                             const QList<AgentToolSpec> &availableTools) const;
    TrustDecision assessTrust(const QString &input,
                              const InputRouteDecision &decision,
                              const ToolPlan &plan) const;
    ResponseMode chooseResponseMode(const QString &input,
                                    const InputRouteDecision &decision,
                                    const TrustDecision &trust) const;
    ActionSession createActionSession(const QString &input,
                                      const InputRouteDecision &decision,
                                      const ToolPlan &plan,
                                      const TrustDecision &trust) const;
    bool shouldContinueActionThread(const QString &input,
                                    const InputRouteDecision &decision,
                                    const ActionThread &thread,
                                    qint64 nowMs) const;
    bool isConfirmationReply(const QString &input, const ActionSession &session) const;
    bool isRejectionReply(const QString &input) const;
};
