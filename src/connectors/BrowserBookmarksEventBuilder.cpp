#include "connectors/BrowserBookmarksEventBuilder.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr qint64 kChromiumEpochOffsetUs = 11644473600000000LL;

struct BookmarkCandidate
{
    QString title;
    QString url;
    qint64 sortKey = 0;
};

qint64 parseChromiumDateUs(const QString &value)
{
    bool ok = false;
    const qint64 raw = value.trimmed().toLongLong(&ok);
    if (!ok || raw <= 0) {
        return 0;
    }
    return raw - kChromiumEpochOffsetUs;
}

void collectBookmarks(const QJsonObject &node, BookmarkCandidate *best)
{
    const QString type = node.value(QStringLiteral("type")).toString().trimmed().toLower();
    if (type == QStringLiteral("url")) {
        BookmarkCandidate candidate;
        candidate.title = node.value(QStringLiteral("name")).toString().trimmed();
        candidate.url = node.value(QStringLiteral("url")).toString().trimmed();
        candidate.sortKey = parseChromiumDateUs(node.value(QStringLiteral("date_added")).toString());
        if (!candidate.url.isEmpty()
            && (best->url.isEmpty() || candidate.sortKey >= best->sortKey)) {
            *best = candidate;
        }
        return;
    }

    const QJsonArray children = node.value(QStringLiteral("children")).toArray();
    for (const QJsonValue &childValue : children) {
        if (childValue.isObject()) {
            collectBookmarks(childValue.toObject(), best);
        }
    }
}
}

ConnectorEvent BrowserBookmarksEventBuilder::fromBookmarksFile(const QString &filePath,
                                                               const QString &browserName,
                                                               const QDateTime &lastModifiedUtc,
                                                               const QString &defaultPriority)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject roots = document.object().value(QStringLiteral("roots")).toObject();
    BookmarkCandidate best;
    for (const QString &rootName : {QStringLiteral("bookmark_bar"),
                                    QStringLiteral("other"),
                                    QStringLiteral("synced")}) {
        const QJsonObject node = roots.value(rootName).toObject();
        if (!node.isEmpty()) {
            collectBookmarks(node, &best);
        }
    }

    if (best.url.trimmed().isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(
        (browserName + QChar::fromLatin1('|') + best.url + QChar::fromLatin1('|') + best.title).toUtf8(),
        QCryptographicHash::Sha1).toHex();

    ConnectorEvent event;
    event.eventId = QString::fromLatin1(digest.left(16));
    event.sourceKind = QStringLiteral("connector_research_browser");
    event.connectorKind = QStringLiteral("research");
    event.taskType = QStringLiteral("web_search");
    event.summary = QStringLiteral("Research updated: %1")
                        .arg(best.title.isEmpty() ? best.url : best.title);
    event.taskKey = QStringLiteral("research:%1").arg(browserName.trimmed().toLower());
    event.itemCount = 1;
    event.priority = defaultPriority.trimmed().isEmpty() ? QStringLiteral("medium") : defaultPriority.trimmed().toLower();
    event.occurredAtUtc = lastModifiedUtc.isValid() ? lastModifiedUtc : QDateTime::currentDateTimeUtc();
    event.metadata = {
        {QStringLiteral("producer"), QStringLiteral("connector_research_browser_monitor")},
        {QStringLiteral("browser"), browserName},
        {QStringLiteral("bookmarkTitle"), best.title},
        {QStringLiteral("bookmarkUrl"), best.url},
        {QStringLiteral("filePath"), info.absoluteFilePath()}
    };
    return event;
}
