#include "core/InputRouter.h"

#include "core/AssistantBehaviorPolicy.h"

InputRouter::InputRouter(const AssistantBehaviorPolicy *behaviorPolicy)
    : m_behaviorPolicy(behaviorPolicy)
{
}

InputRouteDecision InputRouter::decide(const InputRouterContext &context) const
{
    if (m_behaviorPolicy != nullptr) {
        return m_behaviorPolicy->decideRoute(context);
    }

    InputRouteDecision decision;
    decision.kind = InputRouteKind::Conversation;
    decision.useVisionContext = context.visionRelevant;
    decision.intent = context.effectiveIntent;
    return decision;
}
