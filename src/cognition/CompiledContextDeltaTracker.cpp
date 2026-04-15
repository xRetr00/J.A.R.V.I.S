#include "cognition/CompiledContextDeltaTracker.h"

#include <QSet>

CompiledContextDelta CompiledContextDeltaTracker::evaluate(const QString &previousSummary,
                                                           const QStringList &previousKeys,
                                                           const QString &currentSummary,
                                                           const QStringList &currentKeys)
{
    CompiledContextDelta delta;
    delta.previousSummary = previousSummary.trimmed();
    delta.currentSummary = currentSummary.trimmed();
    delta.previousKeys = previousKeys;
    delta.currentKeys = currentKeys;
    delta.summaryChanged = delta.previousSummary != delta.currentSummary;

    const QSet<QString> previousSet(previousKeys.begin(), previousKeys.end());
    const QSet<QString> currentSet(currentKeys.begin(), currentKeys.end());

    for (const QString &key : currentKeys) {
        if (!previousSet.contains(key) && !delta.addedKeys.contains(key)) {
            delta.addedKeys.push_back(key);
        }
    }
    for (const QString &key : previousKeys) {
        if (!currentSet.contains(key) && !delta.removedKeys.contains(key)) {
            delta.removedKeys.push_back(key);
        }
    }

    return delta;
}
