#include "core/ActionThreadTracker.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

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

QString stateToString(ActionThreadState state)
{
    switch (state) {
    case ActionThreadState::Running:
        return QStringLiteral("running");
    case ActionThreadState::Completed:
        return QStringLiteral("completed");
    case ActionThreadState::Failed:
        return QStringLiteral("failed");
    case ActionThreadState::Canceled:
        return QStringLiteral("canceled");
    case ActionThreadState::None:
    default:
        return QStringLiteral("none");
    }
}

ActionThreadState stateFromString(const QString &value)
{
    const QString normalized = value.toLower().trimmed();
    if (normalized == QStringLiteral("running")) {
        return ActionThreadState::Running;
    }
    if (normalized == QStringLiteral("completed")) {
        return ActionThreadState::Completed;
    }
    if (normalized == QStringLiteral("failed")) {
        return ActionThreadState::Failed;
    }
    if (normalized == QStringLiteral("canceled")) {
        return ActionThreadState::Canceled;
    }
    return ActionThreadState::None;
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.push_back(value);
    }
    return array;
}

QStringList stringListFromJson(const QJsonArray &array)
{
    QStringList values;
    for (const QJsonValue &value : array) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.push_back(text);
        }
    }
    return values;
}

QJsonObject threadToJson(const ActionThread &thread)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), thread.id);
    object.insert(QStringLiteral("taskType"), thread.taskType);
    object.insert(QStringLiteral("userGoal"), thread.userGoal);
    object.insert(QStringLiteral("resultSummary"), thread.resultSummary);
    object.insert(QStringLiteral("artifactText"), thread.artifactText);
    object.insert(QStringLiteral("payload"), thread.payload);
    object.insert(QStringLiteral("sourceUrls"), stringListToJson(thread.sourceUrls));
    object.insert(QStringLiteral("nextStepHint"), thread.nextStepHint);
    object.insert(QStringLiteral("state"), stateToString(thread.state));
    object.insert(QStringLiteral("success"), thread.success);
    object.insert(QStringLiteral("valid"), thread.valid);
    object.insert(QStringLiteral("updatedAtMs"), QString::number(thread.updatedAtMs));
    object.insert(QStringLiteral("expiresAtMs"), QString::number(thread.expiresAtMs));
    object.insert(QStringLiteral("originalUserGoal"), thread.originalUserGoal);
    object.insert(QStringLiteral("evidenceSummary"), thread.evidenceSummary);
    object.insert(QStringLiteral("evidenceConfidence"), thread.evidenceConfidence);
    object.insert(QStringLiteral("privateContext"), thread.privateContext);
    object.insert(QStringLiteral("failureReason"), thread.failureReason);
    object.insert(QStringLiteral("cancelReason"), thread.cancelReason);
    object.insert(QStringLiteral("supersededByThreadId"), thread.supersededByThreadId);
    object.insert(QStringLiteral("nextStepCandidates"), stringListToJson(thread.nextStepCandidates));
    object.insert(QStringLiteral("contextThreadId"), thread.contextThreadId);
    object.insert(QStringLiteral("createdAtMs"), QString::number(thread.createdAtMs));
    return object;
}

qint64 jsonInt64(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    if (value.isString()) {
        return value.toString().toLongLong();
    }
    return static_cast<qint64>(value.toDouble());
}

ActionThread threadFromJson(const QJsonObject &object)
{
    ActionThread thread;
    thread.id = object.value(QStringLiteral("id")).toString();
    thread.taskType = object.value(QStringLiteral("taskType")).toString();
    thread.userGoal = sanitizeThreadUserGoal(object.value(QStringLiteral("userGoal")).toString());
    thread.resultSummary = sanitizeThreadSummary(object.value(QStringLiteral("resultSummary")).toString());
    thread.artifactText = sanitizeThreadArtifact(object.value(QStringLiteral("artifactText")).toString());
    thread.payload = object.value(QStringLiteral("payload")).toObject();
    thread.sourceUrls = stringListFromJson(object.value(QStringLiteral("sourceUrls")).toArray());
    thread.nextStepHint = sanitizeThreadHint(object.value(QStringLiteral("nextStepHint")).toString());
    thread.state = stateFromString(object.value(QStringLiteral("state")).toString());
    thread.success = object.value(QStringLiteral("success")).toBool();
    thread.valid = object.value(QStringLiteral("valid")).toBool();
    thread.updatedAtMs = jsonInt64(object, QStringLiteral("updatedAtMs"));
    thread.expiresAtMs = jsonInt64(object, QStringLiteral("expiresAtMs"));
    thread.originalUserGoal = sanitizeThreadUserGoal(object.value(QStringLiteral("originalUserGoal")).toString());
    thread.evidenceSummary = sanitizeThreadSummary(object.value(QStringLiteral("evidenceSummary")).toString());
    thread.evidenceConfidence = object.value(QStringLiteral("evidenceConfidence")).toString();
    thread.privateContext = object.value(QStringLiteral("privateContext")).toBool();
    thread.failureReason = sanitizeThreadSummary(object.value(QStringLiteral("failureReason")).toString());
    thread.cancelReason = sanitizeThreadSummary(object.value(QStringLiteral("cancelReason")).toString());
    thread.supersededByThreadId = object.value(QStringLiteral("supersededByThreadId")).toString();
    thread.nextStepCandidates = stringListFromJson(object.value(QStringLiteral("nextStepCandidates")).toArray());
    thread.contextThreadId = object.value(QStringLiteral("contextThreadId")).toString();
    thread.createdAtMs = jsonInt64(object, QStringLiteral("createdAtMs"));
    return thread;
}
}

ActionThreadTracker::ActionThreadTracker() = default;

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

QList<ActionThread> ActionThreadTracker::recentThreads(qint64 nowMs) const
{
    QList<ActionThread> recent;
    for (const ActionThread &thread : m_threads) {
        if (thread.isUsable(nowMs)) {
            recent.push_back(thread);
        }
    }
    std::sort(recent.begin(), recent.end(), [](const ActionThread &left, const ActionThread &right) {
        return left.updatedAtMs > right.updatedAtMs;
    });
    return recent;
}

std::optional<ActionThread> ActionThreadTracker::threadById(const QString &threadId) const
{
    for (const ActionThread &thread : m_threads) {
        if (thread.id == threadId) {
            return thread;
        }
    }
    return std::nullopt;
}

void ActionThreadTracker::begin(const ActionThreadStartContext &context)
{
    ActionThread thread;
    thread.id = context.threadId;
    thread.taskType = context.taskType;
    thread.userGoal = sanitizeThreadUserGoal(context.userGoal);
    thread.originalUserGoal = thread.userGoal;
    thread.resultSummary = sanitizeThreadSummary(context.resultSummary);
    thread.nextStepHint = sanitizeThreadHint(context.nextStepHint);
    thread.state = ActionThreadState::Running;
    thread.success = false;
    thread.valid = true;
    thread.updatedAtMs = context.updatedAtMs;
    thread.expiresAtMs = context.expiresAtMs;
    thread.createdAtMs = context.updatedAtMs;
    thread.evidenceConfidence = QStringLiteral("none");
    storeThread(thread);
}

void ActionThreadTracker::rememberResult(const ActionThreadResultContext &context)
{
    ActionThread thread;
    if (m_current.has_value()) {
        thread = *m_current;
        thread.userGoal = sanitizeThreadUserGoal(thread.userGoal);
        thread.originalUserGoal = sanitizeThreadUserGoal(
            thread.originalUserGoal.trimmed().isEmpty() ? thread.userGoal : thread.originalUserGoal);
    } else {
        thread.id = context.fallbackThreadId;
        thread.userGoal = sanitizeThreadUserGoal(context.fallbackUserGoal);
        thread.originalUserGoal = thread.userGoal;
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
        thread.cancelReason = thread.resultSummary;
    } else if (!context.success) {
        thread.failureReason = thread.resultSummary;
    }
    thread.success = context.success;
    thread.valid = true;
    thread.updatedAtMs = context.updatedAtMs;
    thread.expiresAtMs = context.expiresAtMs;
    if (thread.createdAtMs <= 0) {
        thread.createdAtMs = context.updatedAtMs;
    }
    thread.evidenceSummary = thread.resultSummary;
    thread.evidenceConfidence = context.success && thread.hasArtifacts()
        ? QStringLiteral("medium")
        : (context.success ? QStringLiteral("weak") : QStringLiteral("blocked"));
    storeThread(thread);
}

void ActionThreadTracker::rememberReply(const ActionThreadReplyContext &context)
{
    ActionThread thread;
    thread.id = context.threadId;
    thread.taskType = context.taskType.trimmed().isEmpty() ? QStringLiteral("assistant_action") : context.taskType.trimmed();
    thread.userGoal = sanitizeThreadUserGoal(context.userGoal);
    thread.originalUserGoal = thread.userGoal;
    thread.resultSummary = sanitizeThreadSummary(context.resultSummary);
    thread.nextStepHint = sanitizeThreadHint(context.nextStepHint);
    thread.state = context.success ? ActionThreadState::Completed : ActionThreadState::Failed;
    thread.success = context.success;
    thread.valid = true;
    thread.updatedAtMs = context.updatedAtMs;
    thread.expiresAtMs = context.expiresAtMs;
    thread.createdAtMs = context.updatedAtMs;
    thread.failureReason = context.success ? QString() : thread.resultSummary;
    thread.evidenceSummary = thread.resultSummary;
    thread.evidenceConfidence = context.success ? QStringLiteral("weak") : QStringLiteral("blocked");
    storeThread(thread);
}

void ActionThreadTracker::clear()
{
    m_current.reset();
    save();
}

void ActionThreadTracker::setCurrentThread(const QString &threadId)
{
    const std::optional<ActionThread> thread = threadById(threadId);
    if (thread.has_value()) {
        m_current = thread;
        save();
    }
}

void ActionThreadTracker::markCurrentCanceled(const QString &reason, qint64 nowMs, qint64 expiresAtMs)
{
    if (!m_current.has_value()) {
        return;
    }

    ActionThread thread = *m_current;
    thread.state = ActionThreadState::Canceled;
    thread.success = false;
    thread.valid = true;
    thread.updatedAtMs = nowMs;
    thread.expiresAtMs = expiresAtMs;
    thread.cancelReason = sanitizeThreadSummary(reason.trimmed().isEmpty()
        ? QStringLiteral("Canceled by user.")
        : reason);
    thread.resultSummary = thread.cancelReason;
    thread.evidenceConfidence = QStringLiteral("blocked");
    storeThread(thread);
}

void ActionThreadTracker::enablePersistence(const QString &filePath)
{
    m_persistencePath = filePath;
}

bool ActionThreadTracker::load()
{
    if (m_persistencePath.trimmed().isEmpty()) {
        return false;
    }

    QFile file(m_persistencePath);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = document.object();
    m_threads.clear();
    const QJsonArray array = root.value(QStringLiteral("threads")).toArray();
    for (const QJsonValue &value : array) {
        ActionThread thread = threadFromJson(value.toObject());
        if (thread.valid && thread.state != ActionThreadState::None) {
            m_threads.push_back(thread);
        }
    }
    std::sort(m_threads.begin(), m_threads.end(), [](const ActionThread &left, const ActionThread &right) {
        return left.updatedAtMs > right.updatedAtMs;
    });
    const QString currentId = root.value(QStringLiteral("currentThreadId")).toString();
    m_current.reset();
    if (!currentId.isEmpty()) {
        setCurrentThread(currentId);
    } else if (!m_threads.isEmpty()) {
        m_current = m_threads.first();
    }
    return true;
}

bool ActionThreadTracker::save() const
{
    if (m_persistencePath.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo info(m_persistencePath);
    QDir().mkpath(info.absolutePath());

    QJsonArray array;
    for (const ActionThread &thread : m_threads) {
        array.push_back(threadToJson(thread));
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), QStringLiteral("action_threads.v1"));
    root.insert(QStringLiteral("threads"), array);
    root.insert(QStringLiteral("currentThreadId"), m_current.has_value() ? m_current->id : QString());

    QSaveFile file(m_persistencePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

void ActionThreadTracker::storeThread(const ActionThread &thread)
{
    if (thread.id.trimmed().isEmpty()) {
        return;
    }

    ActionThread sanitized = thread;
    sanitized.userGoal = sanitizeThreadUserGoal(sanitized.userGoal);
    sanitized.originalUserGoal = sanitizeThreadUserGoal(
        sanitized.originalUserGoal.trimmed().isEmpty() ? sanitized.userGoal : sanitized.originalUserGoal);
    sanitized.resultSummary = sanitizeThreadSummary(sanitized.resultSummary);
    sanitized.artifactText = sanitizeThreadArtifact(sanitized.artifactText);
    sanitized.nextStepHint = sanitizeThreadHint(sanitized.nextStepHint);
    sanitized.failureReason = sanitizeThreadSummary(sanitized.failureReason);
    sanitized.cancelReason = sanitizeThreadSummary(sanitized.cancelReason);

    bool replaced = false;
    for (ActionThread &existing : m_threads) {
        if (existing.id == sanitized.id) {
            existing = sanitized;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_threads.push_back(sanitized);
    }

    std::sort(m_threads.begin(), m_threads.end(), [](const ActionThread &left, const ActionThread &right) {
        return left.updatedAtMs > right.updatedAtMs;
    });
    while (m_threads.size() > 12) {
        m_threads.removeLast();
    }
    pruneExpired(sanitized.updatedAtMs);
    m_current = sanitized;
    save();
}

void ActionThreadTracker::pruneExpired(qint64 nowMs)
{
    QList<ActionThread> kept;
    kept.reserve(m_threads.size());
    for (const ActionThread &thread : m_threads) {
        if (thread.expiresAtMs <= 0 || thread.expiresAtMs > nowMs) {
            kept.push_back(thread);
        }
    }
    m_threads = kept;
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
