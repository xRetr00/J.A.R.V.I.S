#pragma once

#include <QDateTime>
#include <QString>

#include "companion/contracts/ConnectorEvent.h"

class NotesDirectoryEventBuilder
{
public:
    [[nodiscard]] static ConnectorEvent fromFile(const QString &filePath,
                                                 const QDateTime &lastModifiedUtc,
                                                 const QString &defaultPriority = QStringLiteral("low"));
};
