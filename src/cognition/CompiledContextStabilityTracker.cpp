#include "cognition/CompiledContextStabilityTracker.h"

CompiledContextStabilitySummary CompiledContextStabilityTracker::evaluate(const QString &currentSummary,
                                                                          const QStringList &currentKeys,
                                                                          int stableCycles,
                                                                          qint64 stableDurationMs)
{
    CompiledContextStabilitySummary summary;
    summary.currentSummary = currentSummary.simplified();
    summary.currentKeys = currentKeys;
    summary.stableCycles = qMax(0, stableCycles);
    summary.stableDurationMs = qMax<qint64>(0, stableDurationMs);
    summary.stableContext = summary.stableCycles > 0 || summary.stableDurationMs >= 30000;

    for (const QString &key : currentKeys) {
        if (key.startsWith(QStringLiteral("desktop_context_"))
            || key.startsWith(QStringLiteral("connector_summary_")))
        {
            summary.stableKeys.push_back(key);
        }
        if (summary.stableKeys.size() >= 5) {
            break;
        }
    }

    if (!summary.stableContext) {
        summary.summaryText = QStringLiteral("Context is still fresh; no long-horizon stability summary yet.");
        return summary;
    }

    QString text = QStringLiteral("Context stable for %1 cycles over %2 ms.")
                       .arg(summary.stableCycles)
                       .arg(summary.stableDurationMs);
    if (!summary.stableKeys.isEmpty()) {
        text += QStringLiteral(" Stable keys: %1.").arg(summary.stableKeys.join(QStringLiteral(", ")));
    }
    if (!summary.currentSummary.isEmpty()) {
        text += QStringLiteral(" Summary: %1").arg(summary.currentSummary.left(200));
    }
    summary.summaryText = text.simplified();
    return summary;
}
