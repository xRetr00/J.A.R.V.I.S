#pragma once

#include <QDateTime>
#include <QString>

#include "companion/contracts/ConnectorEvent.h"

class BrowserBookmarksEventBuilder
{
public:
    [[nodiscard]] static ConnectorEvent fromBookmarksFile(const QString &filePath,
                                                          const QString &browserName,
                                                          const QDateTime &lastModifiedUtc,
                                                          const QString &defaultPriority = QStringLiteral("medium"));
};
