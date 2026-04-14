#pragma once

#include <QString>
#include <QVariantMap>

class WindowsUiAutomationProbe
{
public:
    [[nodiscard]] static QVariantMap probeWindowMetadata(quintptr windowHandleValue,
                                                         const QString &appId);
};
