#pragma once

#include <QVariantMap>

#include "companion/contracts/BehaviorTraceEvent.h"
#include "core/AssistantTypes.h"

class SelectionTelemetryBuilder
{
public:
    [[nodiscard]] static BehaviorTraceEvent toolPlanEvent(const QString &purpose,
                                                          const QString &inputPreview,
                                                          const QVariantMap &desktopContext,
                                                          const QString &desktopSummary,
                                                          const ToolPlan &plan);
    [[nodiscard]] static BehaviorTraceEvent memoryContextEvent(const QString &purpose,
                                                               const QString &inputPreview,
                                                               const QVariantMap &desktopContext,
                                                               const QString &desktopSummary,
                                                               const MemoryContext &memoryContext,
                                                               const QList<MemoryRecord> &compiledContextRecords);
    [[nodiscard]] static BehaviorTraceEvent promptContextEvent(const QString &purpose,
                                                               const QString &inputPreview,
                                                               const QVariantMap &desktopContext,
                                                               const QString &desktopSummary,
                                                               const QString &promptContext,
                                                               const QList<MemoryRecord> &promptContextRecords,
                                                               const QStringList &suppressedPromptContextKeys,
                                                               int stablePromptCycles,
                                                               qint64 stablePromptDurationMs,
                                                               const QString &reasonCode);
    [[nodiscard]] static BehaviorTraceEvent compiledContextDeltaEvent(const QString &purpose,
                                                                      const QString &inputPreview,
                                                                      const QVariantMap &desktopContext,
                                                                      const QString &desktopSummary,
                                                                      const QString &previousSummary,
                                                                      const QStringList &previousKeys,
                                                                      const QString &currentSummary,
                                                                      const QStringList &currentKeys,
                                                                      const QStringList &addedKeys,
                                                                      const QStringList &removedKeys,
                                                                      bool summaryChanged);
    [[nodiscard]] static BehaviorTraceEvent compiledContextStabilityEvent(const QString &purpose,
                                                                          const QString &inputPreview,
                                                                          const QVariantMap &desktopContext,
                                                                          const QString &desktopSummary,
                                                                          const QString &stabilitySummary,
                                                                          const QStringList &stableKeys,
                                                                          int stableCycles,
                                                                          qint64 stableDurationMs,
                                                                          bool stableContext);
    [[nodiscard]] static BehaviorTraceEvent toolExposureEvent(const QString &purpose,
                                                              const QString &inputPreview,
                                                              const QVariantMap &desktopContext,
                                                              const QString &desktopSummary,
                                                              const QList<AgentToolSpec> &tools);

private:
    [[nodiscard]] static QVariantMap basePayload(const QString &purpose,
                                                 const QString &inputPreview,
                                                 const QVariantMap &desktopContext,
                                                 const QString &desktopSummary);
};
