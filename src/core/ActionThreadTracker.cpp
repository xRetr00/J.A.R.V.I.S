#include "core/ActionThreadTracker.h"

namespace {
QString actionThreadStateText(ActionThreadState state)
{
    switch (state) {
    case ActionThreadState::Running:
        return QStringLiteral("running");
    case ActionThreadState::Completed:
        return QStringLiteral("completed");
    case ActionThreadState::Failed:
        return QStringLiteral("failed");
    case ActionThreadState::Canceled:
    case ActionThreadState::None:
    default:
        return QStringLiteral("canceled");
    }
}
}

const std::optional<ActionThread> &ActionThreadTracker::current() const
{
    return m_current;
}

bool ActionThreadTracker::hasCurrent() const
{
    return m_current.has_value();
}

bool ActionThreadTracker::isCurrentUsable(qint64 nowMs) const
{
    return m_current.has_value() && m_current->isUsable(nowMs);
}

void ActionThreadTracker::begin(const ActionThreadStartContext &context)
{
    ActionThread thread;
    thread.id = context.threadId;
    thread.taskType = context.taskType;
    thread.userGoal = context.userGoal;
    thread.resultSummary = context.resultSummary;
    thread.nextStepHint = context.nextStepHint;
    thread.state = ActionThreadState::Running;
    thread.success = false;
    thread.valid = true;
    thread.updatedAtMs = context.updatedAtMs;
    thread.expiresAtMs = context.expiresAtMs;
    m_current = thread;
}

void ActionThreadTracker::rememberResult(const ActionThreadResultContext &context)
{
    ActionThread thread;
    if (m_current.has_value()) {
        thread = *m_current;
    } else {
        thread.id = context.fallbackThreadId;
        thread.userGoal = context.fallbackUserGoal;
    }

    thread.taskType = context.taskType.trimmed().isEmpty() ? thread.taskType : context.taskType.trimmed();
    thread.resultSummary = context.resultSummary;
    thread.artifactText = context.artifactText;
    thread.payload = context.payload;
    thread.sourceUrls = context.sourceUrls;
    thread.nextStepHint = context.nextStepHint;
    thread.state = context.success ? ActionThreadState::Completed : ActionThreadState::Failed;
    if (context.taskState == TaskState::Canceled) {
        thread.state = ActionThreadState::Canceled;
    }
    thread.success = context.success;
    thread.valid = true;
    thread.updatedAtMs = context.updatedAtMs;
    thread.expiresAtMs = context.expiresAtMs;
    m_current = thread;
}

void ActionThreadTracker::rememberReply(const ActionThreadReplyContext &context)
{
    ActionThread thread;
    thread.id = context.threadId;
    thread.taskType = context.taskType.trimmed().isEmpty() ? QStringLiteral("assistant_action") : context.taskType.trimmed();
    thread.userGoal = context.userGoal;
    thread.resultSummary = context.resultSummary.trimmed();
    thread.nextStepHint = context.nextStepHint;
    thread.state = context.success ? ActionThreadState::Completed : ActionThreadState::Failed;
    thread.success = context.success;
    thread.valid = true;
    thread.updatedAtMs = context.updatedAtMs;
    thread.expiresAtMs = context.expiresAtMs;
    m_current = thread;
}

void ActionThreadTracker::clear()
{
    m_current.reset();
}

QString ActionThreadTracker::buildContinuationInput(const QString &userInput) const
{
    if (!m_current.has_value()) {
        return userInput;
    }

    const ActionThread &thread = *m_current;
    const QString sources = thread.sourceUrls.join(QStringLiteral("\n"));
    const QString stateText = actionThreadStateText(thread.state);

    return QStringLiteral(
        "You are continuing the current assistant action thread.\n"
        "Treat the user's message as a follow-up to this task when appropriate. "
        "Only start a brand-new unrelated task if the user clearly asks for one.\n\n"
        "Thread state: %1\n"
        "Task type: %2\n"
        "User goal: %3\n"
        "Result summary: %4\n"
        "Artifacts:\n%5\n"
        "Sources:\n%6\n"
        "Suggested next step: %7\n\n"
        "User follow-up: %8")
        .arg(stateText,
             thread.taskType.trimmed().isEmpty() ? QStringLiteral("task") : thread.taskType.trimmed(),
             thread.userGoal.trimmed().isEmpty() ? QStringLiteral("unknown") : thread.userGoal.trimmed(),
             thread.resultSummary.trimmed().isEmpty() ? QStringLiteral("none") : thread.resultSummary.trimmed(),
             thread.artifactText.trimmed().isEmpty() ? QStringLiteral("none") : thread.artifactText.trimmed(),
             sources.trimmed().isEmpty() ? QStringLiteral("none") : sources.trimmed(),
             thread.nextStepHint.trimmed().isEmpty() ? QStringLiteral("none") : thread.nextStepHint.trimmed(),
             userInput.trimmed());
}

QString ActionThreadTracker::buildCompletionInput(const ActionThread &thread) const
{
    const QString sources = thread.sourceUrls.join(QStringLiteral("\n"));
    return QStringLiteral(
        "A task just completed.\n"
        "Give the user a short useful completion summary grounded only in the task result below. "
        "If there is an obvious next step, suggest one short follow-up.\n\n"
        "Task type: %1\n"
        "User goal: %2\n"
        "Result summary: %3\n"
        "Artifacts:\n%4\n"
        "Sources:\n%5\n"
        "Suggested next step: %6")
        .arg(thread.taskType.trimmed().isEmpty() ? QStringLiteral("task") : thread.taskType.trimmed(),
             thread.userGoal.trimmed().isEmpty() ? QStringLiteral("unknown") : thread.userGoal.trimmed(),
             thread.resultSummary.trimmed().isEmpty() ? QStringLiteral("none") : thread.resultSummary.trimmed(),
             thread.artifactText.trimmed().isEmpty() ? QStringLiteral("none") : thread.artifactText.trimmed(),
             sources.trimmed().isEmpty() ? QStringLiteral("none") : sources.trimmed(),
             thread.nextStepHint.trimmed().isEmpty() ? QStringLiteral("none") : thread.nextStepHint.trimmed());
}
