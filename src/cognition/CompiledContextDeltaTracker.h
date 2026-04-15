#pragma once

#include <QString>
#include <QStringList>

struct CompiledContextDelta {
    QString previousSummary;
    QString currentSummary;
    QStringList previousKeys;
    QStringList currentKeys;
    QStringList addedKeys;
    QStringList removedKeys;
    bool summaryChanged = false;

    [[nodiscard]] bool hasChanges() const
    {
        return summaryChanged || !addedKeys.isEmpty() || !removedKeys.isEmpty();
    }
};

class CompiledContextDeltaTracker
{
public:
    [[nodiscard]] static CompiledContextDelta evaluate(const QString &previousSummary,
                                                       const QStringList &previousKeys,
                                                       const QString &currentSummary,
                                                       const QStringList &currentKeys);
};
