#include "core/ActionRiskPermissionService.h"

#include <QUuid>

namespace {
QString riskLevel(const ToolPlan &plan, const TrustDecision &trust)
{
    if (trust.requiresConfirmation || trust.highRisk) {
        return QStringLiteral("high");
    }
    if (plan.sideEffecting) {
        return QStringLiteral("medium");
    }
    return QStringLiteral("low");
}

QString riskReason(const ToolPlan &plan, const TrustDecision &trust)
{
    if (!trust.contextReasonCode.trimmed().isEmpty()) {
        return trust.contextReasonCode.trimmed();
    }
    if (trust.requiresConfirmation) {
        return QStringLiteral("risk.confirmation_required");
    }
    if (trust.highRisk || plan.sideEffecting) {
        return QStringLiteral("risk.side_effecting_action");
    }
    if (plan.requiresGrounding) {
        return QStringLiteral("risk.grounding_required");
    }
    return QStringLiteral("risk.low");
}

QVariantList permissionPayload(const QList<PermissionGrant> &permissions)
{
    QVariantList rows;
    for (const PermissionGrant &permission : permissions) {
        rows.push_back(permission.toVariantMap());
    }
    return rows;
}

void applyOverride(PermissionGrant &grant,
                   const PermissionOverrideRule &overrideRule,
                   bool confirmationGranted)
{
    const QString decision = overrideRule.decision.trimmed().toLower();
    if (decision == QStringLiteral("allow")) {
        grant.granted = true;
        grant.scope = overrideRule.scope.trimmed().isEmpty()
            ? QStringLiteral("user_whitelist")
            : overrideRule.scope.trimmed();
        grant.reasonCode = overrideRule.reasonCode.trimmed().isEmpty()
            ? QStringLiteral("permission.allowed_by_user_override")
            : overrideRule.reasonCode.trimmed();
        return;
    }
    if (decision == QStringLiteral("deny")) {
        grant.granted = false;
        grant.scope = overrideRule.scope.trimmed().isEmpty()
            ? QStringLiteral("user_denied")
            : overrideRule.scope.trimmed();
        grant.reasonCode = overrideRule.reasonCode.trimmed().isEmpty()
            ? QStringLiteral("permission.denied_by_user_override")
            : overrideRule.reasonCode.trimmed();
        return;
    }
    if (decision == QStringLiteral("confirm") && !confirmationGranted) {
        grant.granted = false;
        grant.scope = overrideRule.scope.trimmed().isEmpty()
            ? QStringLiteral("pending_confirmation")
            : overrideRule.scope.trimmed();
        grant.reasonCode = overrideRule.reasonCode.trimmed().isEmpty()
            ? QStringLiteral("permission.confirmation_required_by_user_override")
            : overrideRule.reasonCode.trimmed();
    }
}
}

ActionRiskPermissionEvaluation ActionRiskPermissionService::evaluate(const ToolPlan &plan,
                                                                     const TrustDecision &trust,
                                                                     bool confirmationGranted,
                                                                     const QList<PermissionOverrideRule> &overrides)
{
    ActionRiskPermissionEvaluation evaluation;
    evaluation.risk.level = riskLevel(plan, trust);
    evaluation.risk.confirmationRequired = trust.requiresConfirmation;
    evaluation.risk.reasonCode = riskReason(plan, trust);
    evaluation.risk.details = {
        {QStringLiteral("sideEffecting"), plan.sideEffecting},
        {QStringLiteral("requiresGrounding"), plan.requiresGrounding},
        {QStringLiteral("desktopWorkMode"), trust.desktopWorkMode},
        {QStringLiteral("contextReasonCode"), trust.contextReasonCode},
        {QStringLiteral("confirmationGranted"), confirmationGranted},
        {QStringLiteral("toolNames"), plan.orderedToolNames},
        {QStringLiteral("permissionRegistryVersion"), ToolPermissionRegistry::policyVersion()},
        {QStringLiteral("permissionOverrideCount"), overrides.size()},
        {QStringLiteral("trustReason"), trust.reason},
        {QStringLiteral("userMessage"), trust.userMessage}
    };

    for (const QString &toolName : plan.orderedToolNames) {
        for (const ToolPermissionRule &rule : ToolPermissionRegistry::rulesForTool(toolName)) {
            PermissionGrant grant;
            grant.capabilityId = rule.capabilityId;
            grant.granted = confirmationGranted || !trust.requiresConfirmation;
            grant.scope = grant.granted ? rule.defaultScope : QStringLiteral("pending_confirmation");
            grant.reasonCode = grant.granted
                ? QStringLiteral("permission.allowed_by_registry")
                : QStringLiteral("permission.waiting_for_confirmation");
            const PermissionOverrideRule overrideRule =
                ToolPermissionRegistry::overrideForCapability(overrides, grant.capabilityId);
            if (!overrideRule.capabilityId.trimmed().isEmpty()) {
                applyOverride(grant, overrideRule, confirmationGranted);
            }
            evaluation.permissions.push_back(grant);
        }
    }

    return evaluation;
}

BehaviorTraceEvent ActionRiskPermissionService::riskEvent(const ActionRiskPermissionEvaluation &evaluation,
                                                          const QString &routeKind,
                                                          const QVariantMap &desktopContext)
{
    QVariantMap payload = evaluation.risk.toVariantMap();
    payload.insert(QStringLiteral("routeKind"), routeKind);
    payload.insert(QStringLiteral("permissionCount"), evaluation.permissions.size());
    payload.insert(QStringLiteral("desktopTaskId"), desktopContext.value(QStringLiteral("taskId")).toString());
    payload.insert(QStringLiteral("desktopThreadId"), desktopContext.value(QStringLiteral("threadId")).toString());

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("risk_check"),
        QStringLiteral("evaluated"),
        evaluation.risk.reasonCode,
        payload,
        QStringLiteral("system"));
    event.traceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    event.capabilityId = QStringLiteral("action_risk_permission_service");
    return event;
}

BehaviorTraceEvent ActionRiskPermissionService::permissionEvent(const ActionRiskPermissionEvaluation &evaluation,
                                                                const QString &routeKind,
                                                                const QVariantMap &desktopContext)
{
    const QString reason = evaluation.permissions.isEmpty()
        ? QStringLiteral("permission.no_capability_required")
        : QStringLiteral("permission.checked");
    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("permission"),
        QStringLiteral("checked"),
        reason,
        {
            {QStringLiteral("routeKind"), routeKind},
            {QStringLiteral("permissions"), permissionPayload(evaluation.permissions)},
            {QStringLiteral("riskLevel"), evaluation.risk.level},
            {QStringLiteral("confirmationRequired"), evaluation.risk.confirmationRequired},
            {QStringLiteral("permissionRegistryVersion"), ToolPermissionRegistry::policyVersion()}
        },
        QStringLiteral("system"));
    event.traceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    event.capabilityId = QStringLiteral("action_risk_permission_service");
    return event;
}

BehaviorTraceEvent ActionRiskPermissionService::confirmationOutcomeEvent(const ActionRiskPermissionEvaluation &evaluation,
                                                                         const QString &routeKind,
                                                                         const ActionSession &session,
                                                                         const QString &outcome,
                                                                         const QString &inputPreview,
                                                                         const QVariantMap &desktopContext)
{
    const QString normalizedOutcome = outcome.trimmed().isEmpty()
        ? QStringLiteral("unknown")
        : outcome.trimmed().toLower();
    const QString reasonCode = QStringLiteral("confirmation.%1").arg(normalizedOutcome);
    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("confirmation"),
        normalizedOutcome,
        reasonCode,
        {
            {QStringLiteral("routeKind"), routeKind},
            {QStringLiteral("actionSessionId"), session.id},
            {QStringLiteral("userRequest"), session.userRequest.left(180)},
            {QStringLiteral("inputPreview"), inputPreview.left(120)},
            {QStringLiteral("permissions"), permissionPayload(evaluation.permissions)},
            {QStringLiteral("riskLevel"), evaluation.risk.level},
            {QStringLiteral("riskReasonCode"), evaluation.risk.reasonCode},
            {QStringLiteral("executionWillContinue"), normalizedOutcome == QStringLiteral("approved")},
            {QStringLiteral("permissionRegistryVersion"), ToolPermissionRegistry::policyVersion()}
        },
        QStringLiteral("user"));
    event.sessionId = session.id;
    event.traceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    event.capabilityId = QStringLiteral("action_risk_permission_service");
    return event;
}
