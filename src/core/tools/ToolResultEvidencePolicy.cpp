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

bool containsAny(const QString &text, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (text.contains(needle)) {
            return true;
        }
    }
    return false;
}

QString combinedEvidenceText(const ToolExecutionResult &result, const QString &modelOutput)
{
    QStringList chunks;
    chunks << modelOutput;
    chunks << result.summary;
    chunks << result.detail;
    for (const QString &key : result.payload.keys()) {
        const QJsonValue value = result.payload.value(key);
        if (value.isString()) {
            chunks << value.toString();
        }
    }
    return chunks.join(QStringLiteral("\n")).trimmed();
}

bool looksLikeRawMarkupOrSearchShell(const QString &text)
{
    const QString lowered = text.left(8000).toLower();
    if (lowered.isEmpty()) {
        return false;
    }
    if (containsAny(lowered, {
            QStringLiteral("<html"),
            QStringLiteral("<script"),
            QStringLiteral("<body"),
            QStringLiteral("enable javascript"),
            QStringLiteral("unusual traffic"),
            QStringLiteral("captcha"),
            QStringLiteral("search?q=")
        })) {
        return true;
    }

    const int tagLikeCount = lowered.count(QLatin1Char('<')) + lowered.count(QLatin1Char('>'));
    return lowered.size() > 500 && tagLikeCount > lowered.size() / 30;
}

QString confidenceForSuccessfulTool(const ToolExecutionResult &result, int outputChars)
{
    if (result.toolName == QStringLiteral("browser_open")
        || result.toolName == QStringLiteral("computer_open_url")) {
        return QStringLiteral("weak");
    }
    if (hasSources(result.payload)) {
        return QStringLiteral("strong");
    }
    if (!payloadString(result.payload, QStringLiteral("text")).isEmpty()
        || !payloadString(result.payload, QStringLiteral("content")).isEmpty()
        || !payloadString(result.payload, QStringLiteral("summary")).isEmpty()
        || !payloadString(result.payload, QStringLiteral("json")).isEmpty()
        || outputChars > 200) {
        return QStringLiteral("medium");
    }
    return QStringLiteral("weak");
}
}

ToolResultEvidenceAssessment ToolResultEvidencePolicy::assess(const ToolExecutionResult &result,
                                                              const QString &modelOutput)
{
    ToolResultEvidenceAssessment assessment;
    assessment.outputChars = modelOutput.trimmed().size();
    assessment.payloadKeys = sortedPayloadKeys(result.payload);
    assessment.confidence = QStringLiteral("none");

    if (!result.success) {
        assessment.lowSignal = true;
        assessment.lowSignalReason = QStringLiteral("tool_result.blocked_or_failed");
        assessment.confidence = QStringLiteral("blocked");
        return assessment;
    }

    assessment.confidence = confidenceForSuccessfulTool(result, assessment.outputChars);

    if (result.toolName == QStringLiteral("browser_fetch_text") && isBrowserTextEmpty(result.payload)) {
        assessment.lowSignal = true;
        assessment.lowSignalReason = QStringLiteral("tool_result.empty_browser_text");
        assessment.confidence = QStringLiteral("weak");
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
            assessment.confidence = QStringLiteral("weak");
            return assessment;
        }
    }

    if (looksLikeRawMarkupOrSearchShell(combinedEvidenceText(result, modelOutput))) {
        assessment.lowSignal = true;
        assessment.lowSignalReason = QStringLiteral("tool_result.raw_search_or_browser_html");
        assessment.confidence = QStringLiteral("weak");
        return assessment;
    }

    if (isEvidenceTool(result.toolName) && modelOutput.trimmed().isEmpty()) {
        assessment.lowSignal = true;
        assessment.lowSignalReason = QStringLiteral("tool_result.empty_model_output");
        assessment.confidence = QStringLiteral("weak");
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
