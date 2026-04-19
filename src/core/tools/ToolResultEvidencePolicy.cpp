#include "core/tools/ToolResultEvidencePolicy.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonObject>

namespace {
QStringList sortedPayloadKeys(const QJsonObject &payload)
{
    QStringList keys = payload.keys();
    std::sort(keys.begin(), keys.end());
    return keys;
}

QString payloadString(const QJsonObject &payload, const QString &key)
{
    return payload.value(key).toString().trimmed();
}

bool hasSources(const QJsonObject &payload)
{
    return !payload.value(QStringLiteral("sources")).toArray().isEmpty();
}

bool isBrowserTextEmpty(const QJsonObject &payload)
{
    return payloadString(payload, QStringLiteral("text")).isEmpty()
        && payloadString(payload, QStringLiteral("content")).isEmpty();
}

bool isEvidenceTool(const QString &toolName)
{
    return toolName == QStringLiteral("browser_fetch_text")
        || toolName == QStringLiteral("web_search")
        || toolName == QStringLiteral("web_fetch")
        || toolName == QStringLiteral("file_read")
        || toolName == QStringLiteral("file_search")
        || toolName == QStringLiteral("dir_list")
        || toolName == QStringLiteral("log_tail")
        || toolName == QStringLiteral("log_search")
        || toolName == QStringLiteral("ai_log_read");
}
}

ToolResultEvidenceAssessment ToolResultEvidencePolicy::assess(const ToolExecutionResult &result,
                                                              const QString &modelOutput)
{
    ToolResultEvidenceAssessment assessment;
    assessment.outputChars = modelOutput.trimmed().size();
    assessment.payloadKeys = sortedPayloadKeys(result.payload);

    if (!result.success) {
        return assessment;
    }

    if (result.toolName == QStringLiteral("browser_fetch_text") && isBrowserTextEmpty(result.payload)) {
        assessment.lowSignal = true;
        assessment.lowSignalReason = QStringLiteral("tool_result.empty_browser_text");
        return assessment;
    }

    if (result.toolName == QStringLiteral("web_search")) {
        const bool hasSearchPayload = !payloadString(result.payload, QStringLiteral("text")).isEmpty()
            || !payloadString(result.payload, QStringLiteral("content")).isEmpty()
            || !payloadString(result.payload, QStringLiteral("summary")).isEmpty()
            || !payloadString(result.payload, QStringLiteral("json")).isEmpty()
            || hasSources(result.payload);
        if (!hasSearchPayload) {
            assessment.lowSignal = true;
            assessment.lowSignalReason = QStringLiteral("tool_result.empty_web_search");
            return assessment;
        }
    }

    if (isEvidenceTool(result.toolName) && modelOutput.trimmed().isEmpty()) {
        assessment.lowSignal = true;
        assessment.lowSignalReason = QStringLiteral("tool_result.empty_model_output");
    }

    return assessment;
}

ToolResultEvidenceAssessment ToolResultEvidencePolicy::assess(const AgentToolResult &result)
{
    ToolExecutionResult executionResult;
    executionResult.toolName = result.toolName;
    executionResult.callId = result.callId;
    executionResult.success = result.success;
    executionResult.errorKind = result.errorKind;
    executionResult.summary = result.summary;
    executionResult.detail = result.detail;
    executionResult.payload = result.payload;
    executionResult.rawProviderError = result.rawProviderError;
    return assess(executionResult, result.output);
}
