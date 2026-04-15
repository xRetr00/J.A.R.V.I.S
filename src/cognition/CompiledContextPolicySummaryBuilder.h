#pragma once

#include <QHash>
#include <QList>
#include <QVariantMap>

#include "core/AssistantTypes.h"

class CompiledContextPolicySummaryBuilder
{
public:
    [[nodiscard]] static QList<MemoryRecord> build(const QVariantMap &policyState,
                                                   const QHash<QString, QVariantMap> &connectorStateMap);
};
