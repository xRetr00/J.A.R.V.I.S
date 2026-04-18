#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

#include "companion/contracts/BehaviorTraceEvent.h"
#include "companion/contracts/PermissionGrant.h"
#include "companion/contracts/RiskAssessment.h"
#include "core/AssistantTypes.h"
#include "core/ToolPermissionRegistry.h"

struct ActionRiskPermissionEvaluation
{
    RiskAssessment risk;
    QList<PermissionGrant> permissions;
};

class ActionRiskPermissionService
{
public:
    [[nodiscard]] static ActionRiskPermissionEvaluation evaluate(const ToolPlan &plan,
                                                                const TrustDecision &trust,
                                                                bool confirmationGranted,
                                                                const QList<PermissionOverrideRule> &overrides = {});
    [[nodiscard]] static BehaviorTraceEvent riskEvent(const ActionRiskPermissionEvaluation &evaluation,
                                                      const QString &routeKind,
                                                      const QVariantMap &desktopContext);
    [[nodiscard]] static BehaviorTraceEvent permissionEvent(const ActionRiskPermissionEvaluation &evaluation,
                                                            const QString &routeKind,
                                                            const QVariantMap &desktopContext);
    [[nodiscard]] static BehaviorTraceEvent confirmationOutcomeEvent(const ActionRiskPermissionEvaluation &evaluation,
                                                                     const QString &routeKind,
                                                                     const ActionSession &session,
                                                                     const QString &outcome,
                                                                     const QString &inputPreview,
                                                                     const QVariantMap &desktopContext);
};
