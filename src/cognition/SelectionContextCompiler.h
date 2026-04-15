#pragma once

#include <QList>
#include <QVariantMap>

#include "core/AssistantTypes.h"

class AssistantBehaviorPolicy;
class MemoryPolicyHandler;

struct SelectionContextCompilation {
    QList<MemoryRecord> selectedMemoryRecords;
    QList<MemoryRecord> compiledContextRecords;
    MemoryContext memoryContext;
};

class SelectionContextCompiler
{
public:
    [[nodiscard]] static SelectionContextCompilation compile(const QString &query,
                                                             const QVariantMap &desktopContext,
                                                             const QString &desktopSummary,
                                                             const MemoryRecord &runtimeRecord,
                                                             const MemoryPolicyHandler *memoryPolicyHandler,
                                                             const AssistantBehaviorPolicy *behaviorPolicy);
};
