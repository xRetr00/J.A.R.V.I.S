#include "core/DesktopActionContextPolicy.h"

#include "cognition/DesktopWorkMode.h"

namespace {
bool isFocusedWorkMode(const QString &mode)
{
    return mode == QStringLiteral("coding")
        || mode == QStringLiteral("document_editing")
        || mode == QStringLiteral("technical_research")
        || mode == QStringLiteral("web_research");
}

QString workModeLabel(const QString &mode)
{
    if (mode == QStringLiteral("coding")) {
        return QStringLiteral("coding");
    }
    if (mode == QStringLiteral("document_editing")) {
        return QStringLiteral("document work");
    }
    if (mode == QStringLiteral("technical_research")) {
        return QStringLiteral("technical research");
    }
    if (mode == QStringLiteral("web_research")) {
        return QStringLiteral("browser research");
    }
    return mode;
}
}

TrustDecision DesktopActionContextPolicy::applyToTrust(const QVariantMap &desktopContext,
                                                       const ToolPlan &plan,
                                                       const TrustDecision &trust)
{
    TrustDecision result = trust;
    const QString workMode = DesktopWorkMode::inferFromContext(desktopContext);
    if (workMode.isEmpty()) {
        return result;
    }

    result.desktopWorkMode = workMode;

    if (workMode == QStringLiteral("private") && plan.sideEffecting) {
        result.highRisk = true;
        result.requiresConfirmation = true;
        result.contextReasonCode = QStringLiteral("action_policy.private_mode_confirmation");
        result.reason = QStringLiteral("Private Mode hides current desktop details, so state-changing actions require explicit confirmation.");
        result.userMessage = QStringLiteral("Private Mode is hiding the current app context, so confirm before I change or open anything.");
        return result;
    }

    if (!isFocusedWorkMode(workMode)) {
        return result;
    }

    const QString label = workModeLabel(workMode);
    if (result.requiresConfirmation) {
        result.contextReasonCode = QStringLiteral("action_policy.focused_work_confirmation");
        result.reason = QStringLiteral("The request changes local state while the desktop context looks like %1.").arg(label);
        result.userMessage = QStringLiteral("You're in %1, so confirm before I change local state.").arg(label);
    } else if (result.highRisk) {
        result.contextReasonCode = QStringLiteral("action_policy.focused_work_high_risk");
        if (result.reason.trimmed().isEmpty()) {
            result.reason = QStringLiteral("The request changes local state while the desktop context looks like %1.").arg(label);
        }
    } else {
        result.contextReasonCode = QStringLiteral("action_policy.focused_work_quiet_progress");
    }

    return result;
}

bool DesktopActionContextPolicy::shouldQuietProgress(const QVariantMap &desktopContext,
                                                     const TrustDecision &trust)
{
    if (trust.highRisk || trust.requiresConfirmation) {
        return false;
    }

    return isFocusedWorkMode(DesktopWorkMode::inferFromContext(desktopContext));
}
