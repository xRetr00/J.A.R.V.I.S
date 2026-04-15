#pragma once

#include <QString>
#include <QStringList>

struct CompiledContextStabilitySummary {
    QString currentSummary;
    QStringList currentKeys;
    QStringList stableKeys;
    QString summaryText;
    int stableCycles = 0;
    qint64 stableDurationMs = 0;
    bool stableContext = false;
};

class CompiledContextStabilityTracker
{
public:
    [[nodiscard]] static CompiledContextStabilitySummary evaluate(const QString &currentSummary,
                                                                  const QStringList &currentKeys,
                                                                  int stableCycles,
                                                                  qint64 stableDurationMs);
};
