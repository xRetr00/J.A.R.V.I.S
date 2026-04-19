#pragma once

#include <QString>
#include <QStringList>

#include "core/AssistantTypes.h"

struct ToolResultEvidenceAssessment
{
    bool lowSignal = false;
    QString lowSignalReason;
    QString confidence = QStringLiteral("none");
    int outputChars = 0;
    QStringList payloadKeys;
};

class ToolResultEvidencePolicy
{
public:
    [[nodiscard]] static ToolResultEvidenceAssessment assess(const ToolExecutionResult &result,
                                                            const QString &modelOutput = {});
    [[nodiscard]] static ToolResultEvidenceAssessment assess(const AgentToolResult &result);
};
