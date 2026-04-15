#include "cognition/PromptContextTemporalPolicy.h"

#include <QSet>

namespace {
QString promptContextFromRecords(const QList<MemoryRecord> &records)
{
    QStringList parts;
    for (const MemoryRecord &record : records) {
        const QString value = record.value.simplified();
        if (!value.isEmpty()) {
            parts.push_back(value);
        }
    }
    return parts.join(QLatin1Char(' ')).simplified();
}
}

PromptContextTemporalDecision PromptContextTemporalPolicy::apply(const QList<MemoryRecord> &records,
                                                                int stableCycles,
                                                                qint64 stableDurationMs)
{
    PromptContextTemporalDecision decision;
    decision.stableCycles = qMax(0, stableCycles);
    decision.stableDurationMs = qMax<qint64>(0, stableDurationMs);
    decision.stableContext = decision.stableCycles > 0 || decision.stableDurationMs >= 30000;

    QSet<QString> suppressed;
    if (decision.stableContext) {
        suppressed.insert(QStringLiteral("desktop_prompt_summary"));
        suppressed.insert(QStringLiteral("desktop_prompt_app"));
    }
    if (decision.stableCycles >= 2 || decision.stableDurationMs >= 120000) {
        suppressed.insert(QStringLiteral("desktop_prompt_thread"));
    }

    for (const MemoryRecord &record : records) {
        if (suppressed.contains(record.key)) {
            if (!decision.suppressedKeys.contains(record.key)) {
                decision.suppressedKeys.push_back(record.key);
            }
            continue;
        }
        decision.selectedRecords.push_back(record);
    }

    if (decision.selectedRecords.isEmpty()) {
        decision.selectedRecords = records;
        decision.suppressedKeys.clear();
    }

    decision.promptContext = promptContextFromRecords(decision.selectedRecords);
    if (decision.promptContext.isEmpty()) {
        decision.promptContext = promptContextFromRecords(records);
    }

    decision.reasonCode = decision.suppressedKeys.isEmpty()
        ? QStringLiteral("prompt_context.full_context")
        : QStringLiteral("prompt_context.stable_context_trimmed");
    return decision;
}
