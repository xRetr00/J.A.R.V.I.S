#pragma once

#include <QString>
#include <QVariantMap>

class DesktopSourceContextAdapter
{
public:
    [[nodiscard]] static QVariantMap augmentMetadata(const QString &appId,
                                                     const QString &windowTitle,
                                                     const QVariantMap &metadata = {});
};
