#include "core/ToolPermissionRegistry.h"

namespace {
QList<PermissionCapabilityInfo> capabilityRegistry()
{
    return {
        {
            QStringLiteral("filesystem_write"),
            QStringLiteral("Filesystem Write"),
            QStringLiteral("Patch, create, or modify local files."),
            QStringLiteral("route_execution")
        },
        {
            QStringLiteral("desktop_automation"),
            QStringLiteral("Desktop Automation"),
            QStringLiteral("Open apps, URLs, browser windows, or desktop surfaces."),
            QStringLiteral("route_execution")
        },
        {
            QStringLiteral("memory_write"),
            QStringLiteral("Memory Write"),
            QStringLiteral("Store, update, or delete local memory records."),
            QStringLiteral("route_execution")
        },
        {
            QStringLiteral("skill_management"),
            QStringLiteral("Skill Management"),
            QStringLiteral("Create, install, or update assistant skills."),
            QStringLiteral("route_execution")
        },
        {
            QStringLiteral("network_grounding"),
            QStringLiteral("Network Grounding"),
            QStringLiteral("Fetch or search online sources for grounded context."),
            QStringLiteral("route_execution")
        }
    };
}

QList<ToolPermissionRule> registry()
{
    return {
        {QStringLiteral("file_write"), QStringLiteral("filesystem_write"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("file_patch"), QStringLiteral("filesystem_write"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("computer_write_file"), QStringLiteral("filesystem_write"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("computer_open_app"), QStringLiteral("desktop_automation"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("computer_open_url"), QStringLiteral("desktop_automation"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("browser_open"), QStringLiteral("desktop_automation"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("memory_write"), QStringLiteral("memory_write"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("memory_delete"), QStringLiteral("memory_write"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("skill_install"), QStringLiteral("skill_management"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("skill_create"), QStringLiteral("skill_management"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("web_search"), QStringLiteral("network_grounding"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("web_fetch"), QStringLiteral("network_grounding"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")},
        {QStringLiteral("browser_fetch_text"), QStringLiteral("network_grounding"), QStringLiteral("route_execution"), QStringLiteral("tool_permission_registry.v1")}
    };
}
}

QList<PermissionCapabilityInfo> ToolPermissionRegistry::capabilityOptions()
{
    return capabilityRegistry();
}

QList<ToolPermissionRule> ToolPermissionRegistry::rulesForTool(const QString &toolName)
{
    QList<ToolPermissionRule> matches;
    const QString normalized = toolName.trimmed();
    if (normalized.isEmpty()) {
        return matches;
    }

    for (const ToolPermissionRule &rule : registry()) {
        if (rule.toolName == normalized) {
            matches.push_back(rule);
        }
    }
    return matches;
}

PermissionOverrideRule ToolPermissionRegistry::overrideForCapability(const QList<PermissionOverrideRule> &overrides,
                                                                     const QString &capabilityId)
{
    const QString normalized = capabilityId.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    for (const PermissionOverrideRule &overrideRule : overrides) {
        if (overrideRule.capabilityId == normalized && !overrideRule.decision.trimmed().isEmpty()) {
            return overrideRule;
        }
    }
    return {};
}

QString ToolPermissionRegistry::policyVersion()
{
    return QStringLiteral("tool_permission_registry.v1");
}
