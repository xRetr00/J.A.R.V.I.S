#include "telemetry/SelectionTelemetryBuilder.h"

#include "cognition/DesktopWorkMode.h"

#include <QStringList>
#include <QVariantList>

namespace {
QVariantList candidatePayload(const QList<ToolPlanStep> &candidates)
{
    QVariantList rows;
    for (int index = 0; index < candidates.size() && index < 4; ++index) {
        const ToolPlanStep &step = candidates.at(index);
        rows.push_back(QVariantMap{
            {QStringLiteral("toolName"), step.toolName},
            {QStringLiteral("affordanceScore"), step.affordanceScore},
            {QStringLiteral("riskScore"), step.riskScore},
            {QStringLiteral("reason"), step.reason}
        });
    }
    return rows;
}

QStringList recordKeys(const QList<MemoryRecord> &records)
{
    QStringList keys;
    for (const MemoryRecord &record : records) {
        const QString key = record.key.trimmed().isEmpty() ? record.type.trimmed() : record.key.trimmed();
        if (!key.isEmpty()) {
            keys.push_back(key);
        }
        if (keys.size() >= 5) {
            break;
        }
    }
    return keys;
}

QStringList toolNames(const QList<AgentToolSpec> &tools)
{
    QStringList names;
    for (const AgentToolSpec &tool : tools) {
        if (!tool.name.trimmed().isEmpty()) {
            names.push_back(tool.name.trimmed());
        }
    }
    return names;
}

QString contextString(const QVariantMap &desktopContext, const QString &key)
{
    return desktopContext.value(key).toString().trimmed();
}
}

QVariantMap SelectionTelemetryBuilder::basePayload(const QString &purpose,
                                                   const QString &inputPreview,
                                                   const QVariantMap &desktopContext,
                                                   const QString &desktopSummary)
{
    return {
        {QStringLiteral("purpose"), purpose},
        {QStringLiteral("inputPreview"), inputPreview.left(160)},
        {QStringLiteral("desktopSummary"), desktopSummary},
        {QStringLiteral("desktopTaskId"), contextString(desktopContext, QStringLiteral("taskId"))},
        {QStringLiteral("desktopThreadId"), contextString(desktopContext, QStringLiteral("threadId"))},
        {QStringLiteral("desktopTopic"), contextString(desktopContext, QStringLiteral("topic"))},
        {QStringLiteral("desktopMetadataClass"), contextString(desktopContext, QStringLiteral("metadataClass"))},
        {QStringLiteral("desktopDocumentKind"), contextString(desktopContext, QStringLiteral("documentKind"))},
        {QStringLiteral("desktopDocumentContext"), contextString(desktopContext, QStringLiteral("documentContext"))},
        {QStringLiteral("desktopSiteContext"), contextString(desktopContext, QStringLiteral("siteContext"))},
        {QStringLiteral("desktopWorkspaceContext"), contextString(desktopContext, QStringLiteral("workspaceContext"))},
        {QStringLiteral("desktopLanguageHint"), contextString(desktopContext, QStringLiteral("languageHint"))},
        {QStringLiteral("desktopWorkMode"), DesktopWorkMode::inferFromContext(desktopContext)}
    };
}

BehaviorTraceEvent SelectionTelemetryBuilder::toolPlanEvent(const QString &purpose,
                                                            const QString &inputPreview,
                                                            const QVariantMap &desktopContext,
                                                            const QString &desktopSummary,
                                                            const ToolPlan &plan)
{
    QVariantMap payload = basePayload(purpose, inputPreview, desktopContext, desktopSummary);
    payload.insert(QStringLiteral("goal"), plan.goal);
    payload.insert(QStringLiteral("rationale"), plan.rationale);
    payload.insert(QStringLiteral("orderedToolNames"), plan.orderedToolNames);
    payload.insert(QStringLiteral("requiresGrounding"), plan.requiresGrounding);
    payload.insert(QStringLiteral("sideEffecting"), plan.sideEffecting);
    payload.insert(QStringLiteral("candidateCount"), plan.candidates.size());
    payload.insert(QStringLiteral("candidates"), candidatePayload(plan.candidates));

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("selection_context"),
        QStringLiteral("tool_plan"),
        QStringLiteral("selection.tool_plan_compiled"),
        payload,
        QStringLiteral("system"));
    event.capabilityId = QStringLiteral("tool_selector");
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    return event;
}

BehaviorTraceEvent SelectionTelemetryBuilder::memoryContextEvent(const QString &purpose,
                                                                 const QString &inputPreview,
                                                                 const QVariantMap &desktopContext,
                                                                 const QString &desktopSummary,
                                                                 const MemoryContext &memoryContext,
                                                                 const QList<MemoryRecord> &compiledContextRecords)
{
    QVariantMap payload = basePayload(purpose, inputPreview, desktopContext, desktopSummary);
    payload.insert(QStringLiteral("profileCount"), memoryContext.profile.size());
    payload.insert(QStringLiteral("activeCommitmentCount"), memoryContext.activeCommitments.size());
    payload.insert(QStringLiteral("episodicCount"), memoryContext.episodic.size());
    payload.insert(QStringLiteral("profileKeys"), recordKeys(memoryContext.profile));
    payload.insert(QStringLiteral("activeCommitmentKeys"), recordKeys(memoryContext.activeCommitments));
    payload.insert(QStringLiteral("episodicKeys"), recordKeys(memoryContext.episodic));

    int connectorMemoryCount = 0;
    int connectorSummaryCount = 0;
    for (const MemoryRecord &record : memoryContext.activeCommitments) {
        if (record.source.compare(QStringLiteral("connector_memory"), Qt::CaseInsensitive) == 0) {
            connectorMemoryCount += 1;
        }
        if (record.source.compare(QStringLiteral("connector_summary"), Qt::CaseInsensitive) == 0) {
            connectorSummaryCount += 1;
        }
    }
    payload.insert(QStringLiteral("connectorMemoryCount"), connectorMemoryCount);
    payload.insert(QStringLiteral("connectorSummaryCount"), connectorSummaryCount);
    payload.insert(QStringLiteral("compiledContextCount"), compiledContextRecords.size());
    payload.insert(QStringLiteral("compiledContextKeys"), recordKeys(compiledContextRecords));

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("selection_context"),
        QStringLiteral("memory_context"),
        QStringLiteral("selection.memory_context_built"),
        payload,
        QStringLiteral("system"));
    event.capabilityId = QStringLiteral("memory_selector");
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    return event;
}

BehaviorTraceEvent SelectionTelemetryBuilder::promptContextEvent(const QString &purpose,
                                                                 const QString &inputPreview,
                                                                 const QVariantMap &desktopContext,
                                                                 const QString &desktopSummary,
                                                                 const QString &promptContext,
                                                                 const QList<MemoryRecord> &promptContextRecords,
                                                                 const QStringList &suppressedPromptContextKeys,
                                                                 int stablePromptCycles,
                                                                 qint64 stablePromptDurationMs,
                                                                 const QString &reasonCode)
{
    QVariantMap payload = basePayload(purpose, inputPreview, desktopContext, desktopSummary);
    payload.insert(QStringLiteral("promptContext"), promptContext.left(240));
    payload.insert(QStringLiteral("promptContextCount"), promptContextRecords.size());
    payload.insert(QStringLiteral("promptContextKeys"), recordKeys(promptContextRecords));
    payload.insert(QStringLiteral("suppressedPromptContextKeys"), suppressedPromptContextKeys);
    payload.insert(QStringLiteral("stablePromptCycles"), stablePromptCycles);
    payload.insert(QStringLiteral("stablePromptDurationMs"), stablePromptDurationMs);

    QStringList promptReasons;
    for (const MemoryRecord &record : promptContextRecords) {
        const QString reason = record.source.trimmed();
        if (!reason.isEmpty() && !promptReasons.contains(reason)) {
            promptReasons.push_back(reason);
        }
        if (promptReasons.size() >= 5) {
            break;
        }
    }
    payload.insert(QStringLiteral("promptContextReasons"), promptReasons);

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("selection_context"),
        QStringLiteral("prompt_context"),
        reasonCode.trimmed().isEmpty()
            ? QStringLiteral("selection.prompt_context_compiled")
            : reasonCode.trimmed(),
        payload,
        QStringLiteral("system"));
    event.capabilityId = QStringLiteral("selection_context_compiler");
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    return event;
}

BehaviorTraceEvent SelectionTelemetryBuilder::toolExposureEvent(const QString &purpose,
                                                                const QString &inputPreview,
                                                                const QVariantMap &desktopContext,
                                                                const QString &desktopSummary,
                                                                const QList<AgentToolSpec> &tools)
{
    QVariantMap payload = basePayload(purpose, inputPreview, desktopContext, desktopSummary);
    payload.insert(QStringLiteral("toolCount"), tools.size());
    payload.insert(QStringLiteral("selectedToolNames"), toolNames(tools));

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("selection_context"),
        QStringLiteral("tool_exposure"),
        QStringLiteral("selection.tools_exposed"),
        payload,
        QStringLiteral("system"));
    event.capabilityId = QStringLiteral("tool_selector");
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    return event;
}

BehaviorTraceEvent SelectionTelemetryBuilder::compiledContextDeltaEvent(const QString &purpose,
                                                                        const QString &inputPreview,
                                                                        const QVariantMap &desktopContext,
                                                                        const QString &desktopSummary,
                                                                        const QString &previousSummary,
                                                                        const QStringList &previousKeys,
                                                                        const QString &currentSummary,
                                                                        const QStringList &currentKeys,
                                                                        const QStringList &addedKeys,
                                                                        const QStringList &removedKeys,
                                                                        bool summaryChanged)
{
    QVariantMap payload = basePayload(purpose, inputPreview, desktopContext, desktopSummary);
    payload.insert(QStringLiteral("previousSummary"), previousSummary);
    payload.insert(QStringLiteral("currentSummary"), currentSummary);
    payload.insert(QStringLiteral("previousKeys"), previousKeys);
    payload.insert(QStringLiteral("currentKeys"), currentKeys);
    payload.insert(QStringLiteral("addedKeys"), addedKeys);
    payload.insert(QStringLiteral("removedKeys"), removedKeys);
    payload.insert(QStringLiteral("summaryChanged"), summaryChanged);

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("selection_context"),
        QStringLiteral("compiled_context_delta"),
        addedKeys.isEmpty() && removedKeys.isEmpty() && !summaryChanged
            ? QStringLiteral("selection.compiled_context_unchanged")
            : QStringLiteral("selection.compiled_context_changed"),
        payload,
        QStringLiteral("system"));
    event.capabilityId = QStringLiteral("selection_context_compiler");
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    return event;
}

BehaviorTraceEvent SelectionTelemetryBuilder::compiledContextStabilityEvent(const QString &purpose,
                                                                            const QString &inputPreview,
                                                                            const QVariantMap &desktopContext,
                                                                            const QString &desktopSummary,
                                                                            const QString &stabilitySummary,
                                                                            const QStringList &stableKeys,
                                                                            int stableCycles,
                                                                            qint64 stableDurationMs,
                                                                            bool stableContext)
{
    QVariantMap payload = basePayload(purpose, inputPreview, desktopContext, desktopSummary);
    payload.insert(QStringLiteral("stabilitySummary"), stabilitySummary.left(240));
    payload.insert(QStringLiteral("stableKeys"), stableKeys);
    payload.insert(QStringLiteral("stableCycles"), stableCycles);
    payload.insert(QStringLiteral("stableDurationMs"), stableDurationMs);
    payload.insert(QStringLiteral("stableContext"), stableContext);

    BehaviorTraceEvent event = BehaviorTraceEvent::create(
        QStringLiteral("selection_context"),
        QStringLiteral("compiled_context_stability"),
        stableContext
            ? QStringLiteral("selection.compiled_context_stable")
            : QStringLiteral("selection.compiled_context_fresh"),
        payload,
        QStringLiteral("system"));
    event.capabilityId = QStringLiteral("selection_context_compiler");
    event.threadId = desktopContext.value(QStringLiteral("threadId")).toString();
    return event;
}
