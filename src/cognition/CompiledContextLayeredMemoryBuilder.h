#pragma once

#include <QHash>
#include <QList>
#include <QVariantMap>

#include "core/AssistantTypes.h"

class CompiledContextLayeredMemoryBuilder
{
public:
    [[nodiscard]] static QList<MemoryRecord> build(const QVariantMap &policyState,
                                                   const QHash<QString, QVariantMap> &connectorStateMap);
};
