#pragma once

#include <QList>
#include <QVariant>
#include <QVariantMap>

#include "core/AssistantTypes.h"

class CompiledContextPolicyTuningSignalBuilder
{
public:
    [[nodiscard]] static QList<MemoryRecord> build(const QVariantList &history);
    [[nodiscard]] static QList<MemoryRecord> buildFromState(const QVariantMap &state);
    [[nodiscard]] static QVariantMap buildState(const QVariantList &history);
    [[nodiscard]] static QVariantMap buildPlannerMetadata(const QVariantList &history);
    [[nodiscard]] static QVariantMap buildPlannerMetadataFromState(const QVariantMap &state);
};
