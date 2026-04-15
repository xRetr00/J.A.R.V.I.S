#include "cognition/SelectionContextCompiler.h"

#include <QDateTime>

#include "core/AssistantBehaviorPolicy.h"
#include "core/MemoryPolicyHandler.h"

namespace {
MemoryContext fallbackMemoryContext(const QList<MemoryRecord> &memoryRecords,
                                    const QList<MemoryRecord> &compiledContextRecords)
{
    MemoryContext context;
    context.activeCommitments = compiledContextRecords;
    for (const MemoryRecord &record : memoryRecords) {
        bool duplicate = false;
        for (const MemoryRecord &existing : context.activeCommitments) {
            if (existing.key == record.key && existing.source == record.source) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            context.episodic.push_back(record);
        }
    }
    return context;
}

void appendIfPresent(QList<MemoryRecord> &records,
                     const QString &key,
                     const QString &value,
                     float confidence,
                     const QString &updatedAt)
{
    const QString trimmedValue = value.trimmed();
    if (trimmedValue.isEmpty()) {
        return;
    }

    records.push_back({
        .type = QStringLiteral("context"),
        .key = key,
        .value = trimmedValue,
        .confidence = confidence,
        .source = QStringLiteral("desktop_context"),
        .updatedAt = updatedAt
    });
}

QList<MemoryRecord> desktopContextRecords(const QVariantMap &desktopContext,
                                          const QString &desktopSummary)
{
    QList<MemoryRecord> records;
    const QString updatedAt = QString::number(QDateTime::currentMSecsSinceEpoch());
    appendIfPresent(records, QStringLiteral("desktop_context_summary"), desktopSummary, 0.92f, updatedAt);
    appendIfPresent(records,
                    QStringLiteral("desktop_context_task"),
                    desktopContext.value(QStringLiteral("taskId")).toString(),
                    0.88f,
                    updatedAt);
    appendIfPresent(records,
                    QStringLiteral("desktop_context_topic"),
                    desktopContext.value(QStringLiteral("topic")).toString(),
                    0.88f,
                    updatedAt);
    appendIfPresent(records,
                    QStringLiteral("desktop_context_document"),
                    desktopContext.value(QStringLiteral("documentContext")).toString(),
                    0.86f,
                    updatedAt);
    appendIfPresent(records,
                    QStringLiteral("desktop_context_site"),
                    desktopContext.value(QStringLiteral("siteContext")).toString(),
                    0.84f,
                    updatedAt);
    appendIfPresent(records,
                    QStringLiteral("desktop_context_workspace"),
                    desktopContext.value(QStringLiteral("workspaceContext")).toString(),
                    0.84f,
                    updatedAt);
    appendIfPresent(records,
                    QStringLiteral("desktop_context_app"),
                    desktopContext.value(QStringLiteral("appId")).toString(),
                    0.8f,
                    updatedAt);
    return records;
}

QList<MemoryRecord> mergedRecords(const QList<MemoryRecord> &compiledContextRecords,
                                  const QList<MemoryRecord> &selectedMemoryRecords)
{
    QList<MemoryRecord> merged = compiledContextRecords;
    for (const MemoryRecord &record : selectedMemoryRecords) {
        bool duplicate = false;
        for (const MemoryRecord &existing : merged) {
            if (existing.key == record.key && existing.source == record.source) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            merged.push_back(record);
        }
    }
    return merged;
}
}

SelectionContextCompilation SelectionContextCompiler::compile(const QString &query,
                                                             const QVariantMap &desktopContext,
                                                             const QString &desktopSummary,
                                                             const MemoryRecord &runtimeRecord,
                                                             const MemoryPolicyHandler *memoryPolicyHandler,
                                                             const AssistantBehaviorPolicy *behaviorPolicy)
{
    SelectionContextCompilation compilation;
    compilation.selectedMemoryRecords = memoryPolicyHandler
        ? memoryPolicyHandler->requestMemory(query, runtimeRecord)
        : QList<MemoryRecord>{};
    compilation.compiledContextRecords = desktopContextRecords(desktopContext, desktopSummary);

    const QList<MemoryRecord> merged = mergedRecords(compilation.compiledContextRecords,
                                                     compilation.selectedMemoryRecords);
    compilation.memoryContext = behaviorPolicy
        ? behaviorPolicy->buildMemoryContext(query, merged)
        : fallbackMemoryContext(compilation.selectedMemoryRecords, compilation.compiledContextRecords);
    return compilation;
}
