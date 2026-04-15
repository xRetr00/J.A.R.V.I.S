#pragma once

#include <QHash>
#include <QList>
#include <QVariantMap>

#include "core/AssistantTypes.h"

class ConnectorContextCompiler
{
public:
    [[nodiscard]] static QList<MemoryRecord> compileSummaries(const QString &query,
                                                              const QHash<QString, QVariantMap> &stateByKey,
                                                              int maxCount = 3);
};
