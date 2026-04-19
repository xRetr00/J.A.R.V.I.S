#pragma once

#include <optional>

#include "core/AssistantTypes.h"

struct ActionThreadStartContext
{
    QString threadId;
    QString taskType;
    QString userGoal;
    QString resultSummary;
    QString nextStepHint;
    qint64 updatedAtMs = 0;
    qint64 expiresAtMs = 0;
};

struct ActionThreadResultContext
{
    QString fallbackThreadId;
    QString fallbackUserGoal;
    QString taskType;
    QString resultSummary;
    QString artifactText;
    QJsonObject payload;
    QStringList sourceUrls;
    QString nextStepHint;
    bool success = false;
    TaskState taskState = TaskState::Finished;
    qint64 updatedAtMs = 0;
    qint64 expiresAtMs = 0;
};

struct ActionThreadReplyContext
{
    QString threadId;
    QString taskType;
    QString userGoal;
    QString resultSummary;
    QString nextStepHint;
    bool success = false;
    qint64 updatedAtMs = 0;
    qint64 expiresAtMs = 0;
};

class ActionThreadTracker
{
public:
    [[nodiscard]] const std::optional<ActionThread> &current() const;
    [[nodiscard]] bool hasCurrent() const;
    [[nodiscard]] bool isCurrentUsable(qint64 nowMs) const;

    void begin(const ActionThreadStartContext &context);
    void rememberResult(const ActionThreadResultContext &context);
    void rememberReply(const ActionThreadReplyContext &context);
    void clear();

    [[nodiscard]] QString buildContinuationInput(const QString &userInput) const;
    [[nodiscard]] QString buildCompletionInput(const ActionThread &thread) const;

private:
    std::optional<ActionThread> m_current;
};
