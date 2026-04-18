#pragma once

#include <QList>
#include <QString>

struct ToolPermissionRule
{
    QString toolName;
    QString capabilityId;
    QString defaultScope;
    QString policySource;
};

struct PermissionOverrideRule
{
    QString capabilityId;
    QString decision;
    QString scope;
    QString reasonCode;
};

struct PermissionCapabilityInfo
{
    QString capabilityId;
    QString label;
    QString description;
    QString defaultScope;
};

class ToolPermissionRegistry
{
public:
    [[nodiscard]] static QList<PermissionCapabilityInfo> capabilityOptions();
    [[nodiscard]] static QList<ToolPermissionRule> rulesForTool(const QString &toolName);
    [[nodiscard]] static PermissionOverrideRule overrideForCapability(const QList<PermissionOverrideRule> &overrides,
                                                                      const QString &capabilityId);
    [[nodiscard]] static QString policyVersion();
};
