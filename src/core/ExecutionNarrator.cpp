#include "core/ExecutionNarrator.h"

#include <QFileInfo>
#include <QUrl>

namespace {
bool isInternalPlanningHint(const QString &hint)
{
    const QString lowered = hint.trimmed().toLower();
    return lowered.contains(QStringLiteral("ground the answer"))
        || lowered.contains(QStringLiteral("retrieved evidence"))
        || lowered.contains(QStringLiteral("inspect and verify state"))
        || lowered.contains(QStringLiteral("side-effecting action"))
        || lowered.contains(QStringLiteral("smallest useful tool surface"))
        || lowered.contains(QStringLiteral("request changes state"))
        || lowered.contains(QStringLiteral("keep execution explicit"));
}

QString firstNonEmptyArg(const QJsonObject &args, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QString value = args.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QString compactText(QString text, int maxLength = 72)
{
    text = text.simplified();
    if (text.size() > maxLength) {
        text = text.left(maxLength - 3).trimmed() + QStringLiteral("...");
    }
    return text;
}

QString compactPath(const QString &pathText)
{
    const QString trimmed = pathText.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QFileInfo info(trimmed);
    const QString fileName = info.fileName().trimmed();
    return compactText(fileName.isEmpty() ? trimmed : fileName, 56);
}

QString compactUrl(const QString &urlText)
{
    const QUrl url(urlText.trimmed());
    if (url.isValid() && !url.host().trimmed().isEmpty()) {
        return compactText(url.host(), 48);
    }
    return compactText(urlText, 48);
}
}

QString ExecutionNarrator::preActionText(const ActionSession &session, const QString &fallback) const
{
    const QString primaryTool = session.selectedTools.isEmpty() ? QString{} : session.selectedTools.first();
    if (session.responseMode == ResponseMode::ActWithProgress || session.responseMode == ResponseMode::Act) {
        if (session.shouldAnnounceProgress && !session.progress.trimmed().isEmpty()) {
            return appendNextStepHint(session.progress.trimmed(), session);
        }
        if (primaryTool == QStringLiteral("web_search")) {
            return QStringLiteral("I'm checking that now and I'll summarize what I find.");
        }
        if (primaryTool == QStringLiteral("file_read") || primaryTool == QStringLiteral("dir_list")) {
            return QStringLiteral("I'm checking that now.");
        }
        if (primaryTool == QStringLiteral("file_write")
            || primaryTool == QStringLiteral("file_patch")
            || primaryTool == QStringLiteral("computer_write_file")) {
            return QStringLiteral("I'm preparing that change now.");
        }
        if (primaryTool == QStringLiteral("memory_write")) {
            return QStringLiteral("I'll keep that in mind.");
        }
        if (primaryTool == QStringLiteral("computer_open_app")
            || primaryTool == QStringLiteral("computer_open_url")
            || primaryTool == QStringLiteral("browser_open")) {
            return QStringLiteral("I'm opening that now.");
        }
        if (primaryTool == QStringLiteral("computer_set_timer")) {
            return QStringLiteral("I'm setting that now.");
        }
    }

    return session.preamble.trimmed().isEmpty() ? fallback : session.preamble;
}

QString ExecutionNarrator::statusForSession(const ActionSession &session, const QString &fallback) const
{
    switch (session.responseMode) {
    case ResponseMode::ActWithProgress:
        return QStringLiteral("Task in progress");
    case ResponseMode::Act:
        return QStringLiteral("Working on request");
    case ResponseMode::Recover:
        return QStringLiteral("Recovering from failure");
    case ResponseMode::Confirm:
        return QStringLiteral("Confirmation needed");
    case ResponseMode::Summarize:
        return QStringLiteral("Direct response");
    case ResponseMode::Clarify:
        return QStringLiteral("Clarification needed");
    case ResponseMode::Chat:
    default:
        return fallback.trimmed().isEmpty() ? QStringLiteral("Response ready") : fallback;
    }
}

QString ExecutionNarrator::confirmationPrompt(const ActionSession &session) const
{
    const QString base = session.preamble.trimmed().isEmpty()
        ? QStringLiteral("This changes local state.")
        : session.preamble.trimmed();
    return appendNextStepHint(
        QStringLiteral("%1 Confirm if you want me to continue.").arg(base),
        session);
}

QString ExecutionNarrator::confirmationCanceled(const ActionSession &session) const
{
    return outcomeSummary(session, false, QStringLiteral("Okay, I won't run that action."));
}

QString ExecutionNarrator::clarificationPrompt(const ActionSession &session, const QString &fallback) const
{
    const QString base = fallback.trimmed().isEmpty()
        ? QStringLiteral("I didn't catch that.")
        : fallback.trimmed();
    return appendNextStepHint(base, session);
}

QString ExecutionNarrator::validationFailure(const ActionSession &session, const QString &fallback) const
{
    const QString base = fallback.trimmed().isEmpty()
        ? QStringLiteral("I couldn't validate the tool step for that request.")
        : fallback.trimmed();
    return appendNextStepHint(base, session);
}

QString ExecutionNarrator::commandSummary(const ActionSession &session, const QString &target, const QString &result) const
{
    const QString base = QStringLiteral("I handled that as a device action for %1. %2")
        .arg(target.trimmed(), result.trimmed());
    return appendNextStepHint(base, session);
}

QString ExecutionNarrator::gestureReply(const QString &kind) const
{
    if (kind == QStringLiteral("farewell")) {
        return QStringLiteral("Bye.");
    }
    if (kind == QStringLiteral("confirm")) {
        return QStringLiteral("Confirmed.");
    }
    if (kind == QStringLiteral("reject")) {
        return QStringLiteral("Canceled.");
    }
    if (kind == QStringLiteral("conversation_end")) {
        return QStringLiteral("Standing by.");
    }
    return QStringLiteral("I didn't catch that.");
}

QString ExecutionNarrator::appendNextStepHint(const QString &text, const ActionSession &session) const
{
    const QString trimmedText = text.trimmed();
    const QString trimmedHint = session.nextStepHint.trimmed();
    if (trimmedHint.isEmpty()
        || isInternalPlanningHint(trimmedHint)
        || session.responseMode == ResponseMode::Chat
        || trimmedText.contains(trimmedHint)) {
        return trimmedText;
    }
    return QStringLiteral("%1 %2").arg(trimmedText, trimmedHint);
}

QString ExecutionNarrator::outcomeSummary(const ActionSession &session, bool success, const QString &fallback) const
{
    const QString preferred = success ? session.successSummary.trimmed() : session.failureSummary.trimmed();
    const QString text = preferred.isEmpty() ? fallback.trimmed() : preferred;
    return appendNextStepHint(text, session);
}

QPair<QString, QString> ExecutionNarrator::describeBackgroundTask(const AgentTask &task) const
{
    const QString type = task.type.trimmed().toLower();
    const QJsonObject &args = task.args;

    if (type == QStringLiteral("web_search")) {
        return {QStringLiteral("Checking the web..."),
                compactText(firstNonEmptyArg(args, {QStringLiteral("query"), QStringLiteral("q")}), 64)};
    }
    if (type == QStringLiteral("dir_list")) {
        return {QStringLiteral("Inspecting the workspace..."),
                compactPath(firstNonEmptyArg(args, {QStringLiteral("path"), QStringLiteral("directory"), QStringLiteral("dir")}))};
    }
    if (type == QStringLiteral("file_read")) {
        return {QStringLiteral("Reading the file..."),
                compactPath(firstNonEmptyArg(args, {QStringLiteral("path"), QStringLiteral("file"), QStringLiteral("filename")}))};
    }
    if (type == QStringLiteral("file_write")
        || type == QStringLiteral("file_patch")
        || type == QStringLiteral("computer_write_file")) {
        return {QStringLiteral("Applying the file change..."),
                compactPath(firstNonEmptyArg(args, {QStringLiteral("path"), QStringLiteral("file"), QStringLiteral("filename")}))};
    }
    if (type == QStringLiteral("memory_write")) {
        return {QStringLiteral("Saving that for later..."),
                compactText(firstNonEmptyArg(args, {QStringLiteral("title"), QStringLiteral("key"), QStringLiteral("content"), QStringLiteral("value")}), 60)};
    }
    if (type == QStringLiteral("computer_open_app")) {
        return {QStringLiteral("Opening the app..."),
                compactText(firstNonEmptyArg(args, {QStringLiteral("target"), QStringLiteral("app"), QStringLiteral("name")}), 48)};
    }
    if (type == QStringLiteral("computer_open_url") || type == QStringLiteral("browser_open")) {
        return {QStringLiteral("Opening the page..."),
                compactUrl(firstNonEmptyArg(args, {QStringLiteral("url"), QStringLiteral("link")}))};
    }
    if (type == QStringLiteral("computer_set_timer")) {
        return {QStringLiteral("Setting the timer..."),
                compactText(firstNonEmptyArg(args, {QStringLiteral("title"), QStringLiteral("message"), QStringLiteral("duration"), QStringLiteral("duration_seconds")}), 48)};
    }

    return {QStringLiteral("Working on it..."),
            compactText(firstNonEmptyArg(args, {QStringLiteral("query"), QStringLiteral("path"), QStringLiteral("file"), QStringLiteral("url"), QStringLiteral("name")}), 56)};
}

QString ExecutionNarrator::summarizeBackgroundResult(const BackgroundTaskResult &result) const
{
    if (result.success) {
        return result.summary.trimmed().isEmpty()
            ? QStringLiteral("That finished successfully.")
            : result.summary.trimmed();
    }
    return result.detail.trimmed().isEmpty()
        ? QStringLiteral("That task failed.")
        : result.detail.trimmed();
}

QString ExecutionNarrator::statusForBackgroundResult(const BackgroundTaskResult &result) const
{
    if (result.success) {
        return QStringLiteral("Task finished");
    }
    return QStringLiteral("Task failed");
}

QString ExecutionNarrator::summarizeToolResult(const AgentToolResult &result) const
{
    if (result.success) {
        return result.summary.trimmed().isEmpty()
            ? QStringLiteral("Tool completed.")
            : result.summary.trimmed();
    }
    return result.detail.trimmed().isEmpty()
        ? QStringLiteral("Tool failed.")
        : result.detail.trimmed();
}

QString ExecutionNarrator::webSearchLowConfidence(const BackgroundTaskResult &result) const
{
    const QString reason = result.payload.value(QStringLiteral("reliability_reason")).toString().trimmed();
    return reason.isEmpty()
        ? QStringLiteral("I couldn't verify reliable web sources for that yet. Please try a more specific query or ask for source details.")
        : QStringLiteral("I couldn't verify reliable web sources for that yet. %1").arg(reason);
}
