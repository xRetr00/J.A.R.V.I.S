#include "core/ActionThreadTracker.h"

#include <QRegularExpression>

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

QString clipThreadText(QString text, int maxChars)
{
    text = text.simplified();
    if (text.size() > maxChars) {
        text = text.left(maxChars).trimmed() + QStringLiteral("...");
    }
    return text;
}

QString stripContinuationEnvelope(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    const QString marker = QStringLiteral("User follow-up:");
    int markerIndex = text.lastIndexOf(marker, -1, Qt::CaseInsensitive);
    if (markerIndex >= 0) {
        text = text.mid(markerIndex + marker.size()).trimmed();
    }

    text.remove(QRegularExpression(
        QStringLiteral("^You are continuing the current assistant action thread\\.?\\s*"),
        QRegularExpression::CaseInsensitiveOption));
    text.remove(QRegularExpression(
        QStringLiteral("^Treat the user's message as a follow-up to this task when appropriate\\.?\\s*Only start a brand-new unrelated task if the user clearly asks for one\\.?\\s*"),
        QRegularExpression::CaseInsensitiveOption));

    markerIndex = text.indexOf(QStringLiteral("Thread state:"), 0, Qt::CaseInsensitive);
    if (markerIndex >= 0) {
        text = text.left(markerIndex).trimmed();
    }

    return text.trimmed();
}

QString sanitizeThreadUserGoal(const QString &goal)
{
    const QString stripped = stripContinuationEnvelope(goal);
    return clipThreadText(stripped, 420);
}

QString sanitizeThreadSummary(const QString &summary)
{
    return clipThreadText(stripContinuationEnvelope(summary), 700);
}

QString sanitizeThreadArtifact(const QString &artifact)
{
    return clipThreadText(stripContinuationEnvelope(artifact), 2400);
}

QString sanitizeThreadHint(const QString &hint)
{
    return clipThreadText(stripContinuationEnvelope(hint), 240);
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
    thread.userGoal = sanitizeThreadUserGoal(context.userGoal);
    thread.resultSummary = sanitizeThreadSummary(context.resultSummary);
    thread.nextStepHint = sanitizeThreadHint(context.nextStepHint);
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
        thread.userGoal = sanitizeThreadUserGoal(thread.userGoal);
    } else {
        thread.id = context.fallbackThreadId;
        thread.userGoal = sanitizeThreadUserGoal(context.fallbackUserGoal);
    }

    thread.taskType = context.taskType.trimmed().isEmpty() ? thread.taskType : context.taskType.trimmed();
    thread.resultSummary = sanitizeThreadSummary(context.resultSummary);
    thread.artifactText = sanitizeThreadArtifact(context.artifactText);
    thread.payload = context.payload;
    thread.sourceUrls = context.sourceUrls;
    thread.nextStepHint = sanitizeThreadHint(context.nextStepHint);
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
    thread.userGoal = sanitizeThreadUserGoal(context.userGoal);
    thread.resultSummary = sanitizeThreadSummary(context.resultSummary);
    thread.nextStepHint = sanitizeThreadHint(context.nextStepHint);
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
    const QString userGoal = sanitizeThreadUserGoal(thread.userGoal);
    const QString resultSummary = sanitizeThreadSummary(thread.resultSummary);
    const QString artifactText = sanitizeThreadArtifact(thread.artifactText);
    const QString nextStepHint = sanitizeThreadHint(thread.nextStepHint);

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
             userGoal.trimmed().isEmpty() ? QStringLiteral("unknown") : userGoal,
             resultSummary.trimmed().isEmpty() ? QStringLiteral("none") : resultSummary,
             artifactText.trimmed().isEmpty() ? QStringLiteral("none") : artifactText,
             sources.trimmed().isEmpty() ? QStringLiteral("none") : sources.trimmed(),
             nextStepHint.trimmed().isEmpty() ? QStringLiteral("none") : nextStepHint,
             userInput.trimmed());
}

QString ActionThreadTracker::buildCompletionInput(const ActionThread &thread) const
{
    const QString sources = thread.sourceUrls.join(QStringLiteral("\n"));
    const QString userGoal = sanitizeThreadUserGoal(thread.userGoal);
    const QString resultSummary = sanitizeThreadSummary(thread.resultSummary);
    const QString artifactText = sanitizeThreadArtifact(thread.artifactText);
    const QString nextStepHint = sanitizeThreadHint(thread.nextStepHint);
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
             userGoal.trimmed().isEmpty() ? QStringLiteral("unknown") : userGoal,
             resultSummary.trimmed().isEmpty() ? QStringLiteral("none") : resultSummary,
             artifactText.trimmed().isEmpty() ? QStringLiteral("none") : artifactText,
             sources.trimmed().isEmpty() ? QStringLiteral("none") : sources.trimmed(),
             nextStepHint.trimmed().isEmpty() ? QStringLiteral("none") : nextStepHint);
}
