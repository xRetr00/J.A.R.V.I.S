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
