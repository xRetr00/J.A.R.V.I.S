#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "core/AssistantTypes.h"

struct PromptContextTemporalDecision {
    QList<MemoryRecord> selectedRecords;
    QString promptContext;
    QStringList suppressedKeys;
    bool stableContext = false;
    int stableCycles = 0;
    qint64 stableDurationMs = 0;
    QString reasonCode;
};

class PromptContextTemporalPolicy
{
public:
    [[nodiscard]] static PromptContextTemporalDecision apply(const QList<MemoryRecord> &records,
                                                             int stableCycles,
                                                             qint64 stableDurationMs);
};
