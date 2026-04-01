#pragma once

#include <QPair>
#include <QString>

#include "core/AssistantTypes.h"

class ExecutionNarrator
{
public:
    QString preActionText(const ActionSession &session, const QString &fallback = QString()) const;
    QString statusForSession(const ActionSession &session, const QString &fallback = QString()) const;
    QString confirmationPrompt(const ActionSession &session) const;
    QString confirmationCanceled(const ActionSession &session) const;
    QString clarificationPrompt(const ActionSession &session, const QString &fallback = QString()) const;
    QString validationFailure(const ActionSession &session, const QString &fallback = QString()) const;
    QString commandSummary(const ActionSession &session, const QString &target, const QString &result) const;
    QString gestureReply(const QString &kind) const;
    QString appendNextStepHint(const QString &text, const ActionSession &session) const;
    QString outcomeSummary(const ActionSession &session, bool success, const QString &fallback = QString()) const;
    QPair<QString, QString> describeBackgroundTask(const AgentTask &task) const;
    QString summarizeBackgroundResult(const BackgroundTaskResult &result) const;
    QString statusForBackgroundResult(const BackgroundTaskResult &result) const;
    QString summarizeToolResult(const AgentToolResult &result) const;
    QString webSearchLowConfidence(const BackgroundTaskResult &result) const;
};
