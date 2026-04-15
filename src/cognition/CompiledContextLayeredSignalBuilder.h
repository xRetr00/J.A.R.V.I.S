#pragma once

#include <QList>
#include <QString>

#include "core/AssistantTypes.h"

class CompiledContextLayeredSignalBuilder
{
public:
    [[nodiscard]] static QString buildSelectionDirective(const QList<MemoryRecord> &records);
    [[nodiscard]] static QString buildPlannerSummary(const QList<MemoryRecord> &records);
    [[nodiscard]] static QStringList buildPlannerKeys(const QList<MemoryRecord> &records);
    [[nodiscard]] static QList<MemoryRecord> buildPromptContextRecords(const QList<MemoryRecord> &records);
};
