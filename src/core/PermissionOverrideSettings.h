#pragma once

#include <QList>
#include <QVariantList>
#include <QVariantMap>

#include "core/ToolPermissionRegistry.h"

class AppSettings;

class PermissionOverrideSettings
{
public:
    [[nodiscard]] static QVariantList sanitize(const QVariantList &rows);
    [[nodiscard]] static QList<PermissionOverrideRule> rulesFromSettings(const AppSettings *settings);
    [[nodiscard]] static QList<PermissionOverrideRule> rulesFromVariantList(const QVariantList &rows);
};
