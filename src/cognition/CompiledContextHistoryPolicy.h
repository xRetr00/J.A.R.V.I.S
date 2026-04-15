#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

#include "core/AssistantTypes.h"

struct CompiledContextHistoryPolicyDecision
{
    QString dominantMode;
    QString selectionDirective;
    QString promptDirective;
    QString reasonCode;
    double strength = 0.0;
    QVariantMap plannerMetadata;

    [[nodiscard]] bool isValid() const
    {
        return !dominantMode.trimmed().isEmpty();
    }
};

class CompiledContextHistoryPolicy
{
public:
    [[nodiscard]] static CompiledContextHistoryPolicyDecision evaluate(const QList<MemoryRecord> &records);
    [[nodiscard]] static MemoryRecord buildContextRecord(const CompiledContextHistoryPolicyDecision &decision);
    [[nodiscard]] static QVariantMap buildState(const CompiledContextHistoryPolicyDecision &decision);
    [[nodiscard]] static CompiledContextHistoryPolicyDecision fromState(const QVariantMap &state);
};
