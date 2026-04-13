#include "perception/DesktopContextThreadBuilder.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include "companion/contracts/ContextThreadId.h"

namespace {
QString firstMeaningfulChunk(const QString &text)
{
    static const QRegularExpression separators(QStringLiteral("\\s*[\\-|:|•|»|/]+\\s*"));
    const QStringList chunks = text.split(separators, Qt::SkipEmptyParts);
    for (QString chunk : chunks) {
        chunk = chunk.simplified();
        if (chunk.size() >= 3) {
            return chunk;
        }
    }
    return text.simplified();
}
}

CompanionContextSnapshot DesktopContextThreadBuilder::fromActiveWindow(const QString &appId,
                                                                       const QString &windowTitle)
{
    CompanionContextSnapshot snapshot;
    snapshot.appId = normalizedAppFamily(appId);
    snapshot.taskId = QStringLiteral("active_window");
    snapshot.topic = inferTopic(windowTitle);
    snapshot.confidence = 0.78;
    snapshot.threadId = ContextThreadId::fromParts(
        {QStringLiteral("desktop"), QStringLiteral("window"), snapshot.appId, snapshot.topic});
    snapshot.metadata.insert(QStringLiteral("windowTitle"), windowTitle.simplified());
    return snapshot;
}

CompanionContextSnapshot DesktopContextThreadBuilder::fromClipboard(const QString &appId,
                                                                    const QString &windowTitle,
                                                                    const QString &clipboardPreview)
{
    CompanionContextSnapshot snapshot;
    snapshot.appId = normalizedAppFamily(appId);
    snapshot.taskId = QStringLiteral("clipboard");
    snapshot.topic = inferTopic(clipboardPreview, windowTitle);
    snapshot.confidence = 0.72;
    snapshot.threadId = ContextThreadId::fromParts(
        {QStringLiteral("desktop"), QStringLiteral("clipboard"), snapshot.appId, snapshot.topic});
    snapshot.metadata.insert(QStringLiteral("windowTitle"), windowTitle.simplified());
    snapshot.metadata.insert(QStringLiteral("clipboardPreview"), clipboardPreview);
    return snapshot;
}

CompanionContextSnapshot DesktopContextThreadBuilder::fromNotification(const QString &title,
                                                                       const QString &message,
                                                                       const QString &priority,
                                                                       const QString &sourceAppId)
{
    CompanionContextSnapshot snapshot;
    snapshot.appId = normalizedAppFamily(sourceAppId);
    snapshot.taskId = QStringLiteral("notification");
    snapshot.topic = inferTopic(title, message);
    snapshot.confidence = priority.trimmed().compare(QStringLiteral("high"), Qt::CaseInsensitive) == 0 ? 0.84 : 0.68;
    snapshot.threadId = ContextThreadId::fromParts(
        {QStringLiteral("desktop"), QStringLiteral("notification"), snapshot.appId, snapshot.topic});
    snapshot.metadata.insert(QStringLiteral("title"), title.simplified());
    snapshot.metadata.insert(QStringLiteral("message"), message.simplified());
    snapshot.metadata.insert(QStringLiteral("priority"), priority.trimmed().toLower());
    return snapshot;
}

QString DesktopContextThreadBuilder::normalizedAppFamily(const QString &appId)
{
    const QString baseName = QFileInfo(appId.trimmed()).completeBaseName();
    QString normalized = normalizeSegment(baseName.isEmpty() ? appId : baseName);
    if (normalized == QStringLiteral("code")) {
        return QStringLiteral("vscode");
    }
    if (normalized == QStringLiteral("msedge")) {
        return QStringLiteral("edge");
    }
    if (normalized.isEmpty()) {
        return QStringLiteral("unknown_app");
    }
    return normalized;
}

QString DesktopContextThreadBuilder::inferTopic(const QString &primaryText, const QString &secondaryText)
{
    QString topic = normalizeSegment(firstMeaningfulChunk(primaryText));
    if (!topic.isEmpty() && topic != QStringLiteral("unknown_topic")) {
        return topic;
    }

    topic = normalizeSegment(firstMeaningfulChunk(secondaryText));
    if (!topic.isEmpty()) {
        return topic;
    }

    return QStringLiteral("unknown_topic");
}

QString DesktopContextThreadBuilder::normalizeSegment(QString value)
{
    value = value.trimmed().toLower();
    if (value.isEmpty()) {
        return {};
    }

    value.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    while (value.startsWith(QLatin1Char('_'))) {
        value.remove(0, 1);
    }
    while (value.endsWith(QLatin1Char('_'))) {
        value.chop(1);
    }
    if (value.size() > 40) {
        value = value.left(40);
        while (value.endsWith(QLatin1Char('_'))) {
            value.chop(1);
        }
    }
    return value;
}
