#include "core/PermissionOverrideSettings.h"

#include <QSet>

#include "settings/AppSettings.h"

namespace {
bool isRegisteredCapability(const QString &capabilityId)
{
    for (const PermissionCapabilityInfo &capability : ToolPermissionRegistry::capabilityOptions()) {
        if (capability.capabilityId == capabilityId) {
            return true;
        }
    }
    return false;
}

QString normalizedDecision(const QVariantMap &row)
{
    const QString decision = row.value(QStringLiteral("decision")).toString().trimmed().toLower();
    if (decision == QStringLiteral("allow")
        || decision == QStringLiteral("deny")
        || decision == QStringLiteral("confirm")) {
        return decision;
    }
    return {};
}
}

QVariantList PermissionOverrideSettings::sanitize(const QVariantList &rows)
{
    QVariantList sanitized;
    QSet<QString> seenCapabilities;

    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();
        const QString capabilityId = row.value(QStringLiteral("capabilityId")).toString().trimmed();
        const QString decision = normalizedDecision(row);
        if (capabilityId.isEmpty()
            || decision.isEmpty()
            || !isRegisteredCapability(capabilityId)
            || seenCapabilities.contains(capabilityId)) {
            continue;
        }

        QVariantMap clean;
        clean.insert(QStringLiteral("capabilityId"), capabilityId.left(96));
        clean.insert(QStringLiteral("decision"), decision);
        clean.insert(QStringLiteral("scope"),
                     row.value(QStringLiteral("scope")).toString().trimmed().left(96));
        clean.insert(QStringLiteral("reasonCode"),
                     row.value(QStringLiteral("reasonCode")).toString().trimmed().left(128));
        sanitized.push_back(clean);
        seenCapabilities.insert(capabilityId);
    }

    return sanitized;
}

QList<PermissionOverrideRule> PermissionOverrideSettings::rulesFromSettings(const AppSettings *settings)
{
    return settings == nullptr ? QList<PermissionOverrideRule>{}
                               : rulesFromVariantList(settings->permissionOverrides());
}

QList<PermissionOverrideRule> PermissionOverrideSettings::rulesFromVariantList(const QVariantList &rows)
{
    QList<PermissionOverrideRule> rules;
    for (const QVariant &value : sanitize(rows)) {
        const QVariantMap row = value.toMap();
        PermissionOverrideRule rule;
        rule.capabilityId = row.value(QStringLiteral("capabilityId")).toString();
        rule.decision = row.value(QStringLiteral("decision")).toString();
        rule.scope = row.value(QStringLiteral("scope")).toString();
        rule.reasonCode = row.value(QStringLiteral("reasonCode")).toString();
        rules.push_back(rule);
    }
    return rules;
}
