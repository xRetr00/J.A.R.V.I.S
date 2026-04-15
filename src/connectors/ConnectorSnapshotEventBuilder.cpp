#include "connectors/ConnectorSnapshotEventBuilder.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString inferConnectorKind(const QFileInfo &info, const QJsonObject &payload)
{
    const QString explicitKind = payload.value(QStringLiteral("connectorKind")).toString().trimmed().toLower();
    if (!explicitKind.isEmpty()) {
        return explicitKind;
    }

    const QString stem = info.completeBaseName().trimmed().toLower();
    if (stem.contains(QStringLiteral("schedule")) || stem.contains(QStringLiteral("calendar"))) {
        return QStringLiteral("schedule");
    }
    if (stem.contains(QStringLiteral("inbox")) || stem.contains(QStringLiteral("mail"))) {
        return QStringLiteral("inbox");
    }
    if (stem.contains(QStringLiteral("note"))) {
        return QStringLiteral("notes");
    }
    if (stem.contains(QStringLiteral("research")) || stem.contains(QStringLiteral("search"))) {
        return QStringLiteral("research");
    }
    return {};
}

QJsonArray payloadItems(const QString &connectorKind, const QJsonObject &payload)
{
    if (connectorKind == QStringLiteral("schedule")) {
        return payload.value(QStringLiteral("events")).toArray();
    }
    if (connectorKind == QStringLiteral("inbox")) {
        return payload.value(QStringLiteral("messages")).toArray();
    }
    if (connectorKind == QStringLiteral("notes")) {
        return payload.value(QStringLiteral("notes")).toArray();
    }
    if (connectorKind == QStringLiteral("research")) {
        return payload.value(QStringLiteral("sources")).toArray();
    }
    return {};
}

QString taskTypeForConnector(const QString &connectorKind)
{
    if (connectorKind == QStringLiteral("schedule")) {
        return QStringLiteral("calendar_review");
    }
    if (connectorKind == QStringLiteral("inbox")) {
        return QStringLiteral("email_fetch");
    }
    if (connectorKind == QStringLiteral("notes")) {
        return QStringLiteral("note_review");
    }
    if (connectorKind == QStringLiteral("research")) {
        return QStringLiteral("web_search");
    }
    return {};
}

QString summaryForConnector(const QString &connectorKind,
                           const QJsonObject &payload,
                           const QJsonArray &items)
{
    if (connectorKind == QStringLiteral("schedule")) {
        const QString title = payload.value(QStringLiteral("title")).toString().trimmed();
        const QString first = items.isEmpty()
            ? QString()
            : items.first().toObject().value(QStringLiteral("title")).toString().trimmed();
        const QString chosen = !title.isEmpty() ? title : first;
        return chosen.isEmpty()
            ? QStringLiteral("Schedule updated.")
            : QStringLiteral("Schedule updated: %1").arg(chosen);
    }
    if (connectorKind == QStringLiteral("inbox")) {
        const int unreadCount = payload.value(QStringLiteral("unreadCount")).toInt(items.size());
        const QString sender = payload.value(QStringLiteral("primarySender")).toString().trimmed();
        if (!sender.isEmpty()) {
            return QStringLiteral("Inbox updated: %1 needs attention.").arg(sender);
        }
        return unreadCount > 0
            ? QStringLiteral("Inbox updated: %1 unread items.").arg(unreadCount)
            : QStringLiteral("Inbox updated.");
    }
    if (connectorKind == QStringLiteral("notes")) {
        const QString title = payload.value(QStringLiteral("title")).toString().trimmed();
        const QString first = items.isEmpty()
            ? QString()
            : items.first().toObject().value(QStringLiteral("title")).toString().trimmed();
        const QString chosen = !title.isEmpty() ? title : first;
        return chosen.isEmpty()
            ? QStringLiteral("Notes updated.")
            : QStringLiteral("Notes updated: %1").arg(chosen);
    }
    if (connectorKind == QStringLiteral("research")) {
        const QString query = payload.value(QStringLiteral("query")).toString().trimmed();
        return query.isEmpty()
            ? QStringLiteral("Research updated.")
            : QStringLiteral("Research updated: %1").arg(query);
    }
    return {};
}
}

ConnectorEvent ConnectorSnapshotEventBuilder::fromSnapshot(const QString &connectorKind,
                                                           const QString &snapshotPath,
                                                           const QByteArray &content,
                                                           const QDateTime &lastModifiedUtc,
                                                           const QString &defaultPriority)
{
    const QJsonDocument document = QJsonDocument::fromJson(content);
    if (!document.isObject()) {
        return {};
    }

    const QFileInfo info(snapshotPath);
    const QJsonObject payload = document.object();
    const QString effectiveKind = connectorKind.trimmed().isEmpty()
        ? inferConnectorKind(info, payload)
        : connectorKind.trimmed().toLower();
    if (effectiveKind.isEmpty()) {
        return {};
    }

    const QJsonArray items = payloadItems(effectiveKind, payload);
    const QString summary = summaryForConnector(effectiveKind, payload, items);
    if (summary.isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(content, QCryptographicHash::Sha1).toHex();
    ConnectorEvent event;
    event.eventId = QString::fromLatin1(digest.left(16));
    event.sourceKind = QStringLiteral("connector_snapshot");
    event.connectorKind = effectiveKind;
    event.taskType = taskTypeForConnector(effectiveKind);
    event.summary = summary;
    event.taskKey = QStringLiteral("%1:%2").arg(effectiveKind, info.completeBaseName().trimmed());
    event.itemCount = items.size();
    event.priority = payload.value(QStringLiteral("priority")).toString().trimmed().isEmpty()
        ? (defaultPriority.trimmed().isEmpty() ? QStringLiteral("medium") : defaultPriority.trimmed().toLower())
        : payload.value(QStringLiteral("priority")).toString().trimmed().toLower();
    event.occurredAtUtc = lastModifiedUtc.isValid() ? lastModifiedUtc : QDateTime::currentDateTimeUtc();
    event.metadata = {
        {QStringLiteral("producer"), QStringLiteral("snapshot_monitor")},
        {QStringLiteral("snapshotPath"), snapshotPath},
        {QStringLiteral("snapshotDigest"), QString::fromLatin1(digest)},
        {QStringLiteral("snapshotModifiedAt"), event.occurredAtUtc.toString(Qt::ISODateWithMs)}
    };

    if (payload.contains(QStringLiteral("unreadCount"))) {
        event.metadata.insert(QStringLiteral("unreadCount"), payload.value(QStringLiteral("unreadCount")).toInt());
    }
    if (payload.contains(QStringLiteral("query"))) {
        event.metadata.insert(QStringLiteral("query"), payload.value(QStringLiteral("query")).toString().trimmed());
    }
    if (payload.contains(QStringLiteral("title"))) {
        event.metadata.insert(QStringLiteral("title"), payload.value(QStringLiteral("title")).toString().trimmed());
    }
    return event;
}

ConnectorEvent ConnectorSnapshotEventBuilder::fromSnapshot(const QString &snapshotPath,
                                                           const QByteArray &content,
                                                           const QDateTime &lastModifiedUtc)
{
    return fromSnapshot(QString{}, snapshotPath, content, lastModifiedUtc, {});
}
