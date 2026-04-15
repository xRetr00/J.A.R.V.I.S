#pragma once

#include <QList>
#include <QVariantMap>

#include "core/AssistantTypes.h"

class AssistantBehaviorPolicy;
class MemoryPolicyHandler;

struct SelectionContextCompilation {
    QString compiledDesktopSummary;
    QString selectionInput;
    QString promptContext;
    QString historySelectionDirective;
    QList<MemoryRecord> selectedMemoryRecords;
    QList<MemoryRecord> compiledContextRecords;
    QList<MemoryRecord> promptContextRecords;
    MemoryContext memoryContext;
    QVariantMap historyPolicyMetadata;
};

class SelectionContextCompiler
{
public:
    [[nodiscard]] static QString buildCompiledDesktopSummary(const QVariantMap &desktopContext,
                                                             const QString &desktopSummary);
    [[nodiscard]] static QString buildSelectionInput(const QString &input,
                                                     IntentType intent,
                                                     const QVariantMap &desktopContext,
                                                     const QString &desktopSummary,
                                                     qint64 desktopContextAtMs,
                                                     bool privateModeEnabled,
                                                     const QList<MemoryRecord> &historyRecords = {});
    [[nodiscard]] static QString buildPromptContext(IntentType intent,
                                                    const QString &desktopSummary,
                                                    const QVariantMap &desktopContext,
                                                    const QList<MemoryRecord> &historyRecords = {});
    [[nodiscard]] static SelectionContextCompilation compile(const QString &query,
                                                             IntentType intent,
                                                             const QVariantMap &desktopContext,
                                                             const QString &desktopSummary,
                                                             qint64 desktopContextAtMs,
                                                             bool privateModeEnabled,
                                                             const MemoryRecord &runtimeRecord,
                                                             const QList<MemoryRecord> &historyRecords,
                                                             const MemoryPolicyHandler *memoryPolicyHandler,
                                                             const AssistantBehaviorPolicy *behaviorPolicy);
};
