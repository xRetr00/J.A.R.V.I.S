#pragma once

#include <QString>
#include <QVariantMap>

struct PlatformCapabilities
{
    QString platformId;
    QString platformLabel;
    bool supportsGlobalHotkey = false;
    bool supportsAppListing = false;
    bool supportsAppLaunch = false;
    bool supportsTimerNotification = false;
    bool supportsAutoToolInstall = false;
};

Q_DECLARE_METATYPE(PlatformCapabilities)

QVariantMap platformCapabilitiesToVariantMap(const PlatformCapabilities &capabilities);
