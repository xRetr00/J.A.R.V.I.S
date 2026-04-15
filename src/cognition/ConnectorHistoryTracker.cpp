#include "cognition/ConnectorHistoryTracker.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

#include "memory/MemoryStore.h"

namespace {
constexpr qint64 kRetentionWindowMs = 24LL * 60LL * 60LL * 1000LL;
constexpr qint64 kRecentWindowMs = 10LL * 60LL * 1000LL;

QString defaultStoragePath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.trimmed().isEmpty()) {
        return {};
    }
    QDir().mkpath(root);
    return root + QStringLiteral("/connector_history.json");
}

QJsonObject toJson(const ConnectorHistoryTracker::Entry &entry)
{
    return {
        {QStringLiteral("sourceKind"), entry.sourceKind},
        {QStringLiteral("connectorKind"), entry.connectorKind},
        {QStringLiteral("firstSeenAtMs"), QString::number(entry.firstSeenAtMs)},
        {QStringLiteral("lastSeenAtMs"), QString::number(entry.lastSeenAtMs)},
        {QStringLiteral("lastPresentedAtMs"), QString::number(entry.lastPresentedAtMs)},
        {QStringLiteral("seenCount"), entry.seenCount},
        {QStringLiteral("presentedCount"), entry.presentedCount}
    };
}

ConnectorHistoryTracker::Entry fromJson(const QJsonObject &object)
{
    ConnectorHistoryTracker::Entry entry;
    entry.sourceKind = object.value(QStringLiteral("sourceKind")).toString().trimmed();
    entry.connectorKind = object.value(QStringLiteral("connectorKind")).toString().trimmed().toLower();
    entry.firstSeenAtMs = object.value(QStringLiteral("firstSeenAtMs")).toString().toLongLong();
    entry.lastSeenAtMs = object.value(QStringLiteral("lastSeenAtMs")).toString().toLongLong();
    entry.lastPresentedAtMs = object.value(QStringLiteral("lastPresentedAtMs")).toString().toLongLong();
    entry.seenCount = object.value(QStringLiteral("seenCount")).toInt();
    entry.presentedCount = object.value(QStringLiteral("presentedCount")).toInt();
    return entry;
}
}

ConnectorHistoryTracker::ConnectorHistoryTracker(MemoryStore *memoryStore, const QString &storagePath)
    : m_memoryStore(memoryStore)
    , m_storagePath(storagePath.trimmed().isEmpty() ? defaultStoragePath() : storagePath.trimmed())
{
}

void ConnectorHistoryTracker::recordSeen(const QString &sourceKind,
                                         const QString &connectorKind,
                                         const QString &historyKey,
                                         qint64 nowMs)
{
    loadIfNeeded();
    if (historyKey.trimmed().isEmpty() || nowMs <= 0) {
        return;
    }

    prune(nowMs);
    Entry &entry = m_entriesByKey[historyKey];
    entry.sourceKind = sourceKind.trimmed();
    entry.connectorKind = connectorKind.trimmed().toLower();
    if (entry.firstSeenAtMs <= 0) {
        entry.firstSeenAtMs = nowMs;
    }
    entry.lastSeenAtMs = nowMs;
    entry.seenCount += 1;
    persist();
}

void ConnectorHistoryTracker::recordPresented(const QString &historyKey, qint64 nowMs)
{
    loadIfNeeded();
    if (historyKey.trimmed().isEmpty() || nowMs <= 0) {
        return;
    }

    prune(nowMs);
    auto it = m_entriesByKey.find(historyKey);
    if (it == m_entriesByKey.end()) {
        return;
    }

    it->lastPresentedAtMs = nowMs;
    it->presentedCount += 1;
    persist();
}

QVariantMap ConnectorHistoryTracker::buildMetadata(const QString &sourceKind,
                                                   const QString &connectorKind,
                                                   const QString &historyKey,
                                                   qint64 nowMs)
{
    loadIfNeeded();
    prune(nowMs);

    const QString normalizedSourceKind = sourceKind.trimmed();
    const QString normalizedConnectorKind = connectorKind.trimmed().toLower();
    const Entry entry = m_entriesByKey.value(historyKey);

    int connectorKindRecentSeenCount = 0;
    int connectorKindRecentPresentedCount = 0;
    int sourceKindRecentSeenCount = 0;
    for (auto it = m_entriesByKey.cbegin(); it != m_entriesByKey.cend(); ++it) {
        const Entry &candidate = it.value();
        const bool recentSeen = candidate.lastSeenAtMs > 0 && (nowMs - candidate.lastSeenAtMs) <= kRecentWindowMs;
        const bool recentPresented = candidate.lastPresentedAtMs > 0 && (nowMs - candidate.lastPresentedAtMs) <= kRecentWindowMs;

        if (recentSeen && candidate.connectorKind == normalizedConnectorKind) {
            connectorKindRecentSeenCount += 1;
        }
        if (recentPresented && candidate.connectorKind == normalizedConnectorKind) {
            connectorKindRecentPresentedCount += 1;
        }
        if (recentSeen && candidate.sourceKind == normalizedSourceKind) {
            sourceKindRecentSeenCount += 1;
        }
    }

    return {
        {QStringLiteral("historyKey"), historyKey},
        {QStringLiteral("historySeenCount"), entry.seenCount},
        {QStringLiteral("historyPresentedCount"), entry.presentedCount},
        {QStringLiteral("historyFirstSeenAtMs"), entry.firstSeenAtMs},
        {QStringLiteral("historyLastSeenAtMs"), entry.lastSeenAtMs},
        {QStringLiteral("historyLastPresentedAtMs"), entry.lastPresentedAtMs},
        {QStringLiteral("historyRecentlySeen"), entry.lastSeenAtMs > 0 && (nowMs - entry.lastSeenAtMs) <= kRecentWindowMs},
        {QStringLiteral("historyRecentlyPresented"), entry.lastPresentedAtMs > 0 && (nowMs - entry.lastPresentedAtMs) <= kRecentWindowMs},
        {QStringLiteral("connectorKindRecentSeenCount"), connectorKindRecentSeenCount},
        {QStringLiteral("connectorKindRecentPresentedCount"), connectorKindRecentPresentedCount},
        {QStringLiteral("sourceKindRecentSeenCount"), sourceKindRecentSeenCount}
    };
}

void ConnectorHistoryTracker::loadIfNeeded()
{
    if (m_loaded) {
        return;
    }
    m_loaded = true;

    if (m_memoryStore != nullptr) {
        const QHash<QString, QVariantMap> entries = m_memoryStore->connectorStateMap();
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            const QVariantMap &map = it.value();
            Entry entry;
            entry.sourceKind = map.value(QStringLiteral("sourceKind")).toString().trimmed();
            entry.connectorKind = map.value(QStringLiteral("connectorKind")).toString().trimmed().toLower();
            entry.firstSeenAtMs = map.value(QStringLiteral("firstSeenAtMs")).toLongLong();
            entry.lastSeenAtMs = map.value(QStringLiteral("lastSeenAtMs")).toLongLong();
            entry.lastPresentedAtMs = map.value(QStringLiteral("lastPresentedAtMs")).toLongLong();
            entry.seenCount = map.value(QStringLiteral("seenCount")).toInt();
            entry.presentedCount = map.value(QStringLiteral("presentedCount")).toInt();
            m_entriesByKey.insert(it.key(), entry);
        }
        return;
    }

    if (m_storagePath.trimmed().isEmpty()) {
        return;
    }

    QFile file(m_storagePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        return;
    }

    const QJsonObject entries = document.object().value(QStringLiteral("entries")).toObject();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        m_entriesByKey.insert(it.key(), fromJson(it.value().toObject()));
    }
}

void ConnectorHistoryTracker::persist() const
{
    if (m_memoryStore != nullptr) {
        QSet<QString> liveKeys;
        for (auto it = m_entriesByKey.cbegin(); it != m_entriesByKey.cend(); ++it) {
            liveKeys.insert(it.key());
            m_memoryStore->upsertConnectorState(it.key(), {
                {QStringLiteral("sourceKind"), it.value().sourceKind},
                {QStringLiteral("connectorKind"), it.value().connectorKind},
                {QStringLiteral("firstSeenAtMs"), it.value().firstSeenAtMs},
                {QStringLiteral("lastSeenAtMs"), it.value().lastSeenAtMs},
                {QStringLiteral("lastPresentedAtMs"), it.value().lastPresentedAtMs},
                {QStringLiteral("seenCount"), it.value().seenCount},
                {QStringLiteral("presentedCount"), it.value().presentedCount}
            });
        }

        const QHash<QString, QVariantMap> persisted = m_memoryStore->connectorStateMap();
        for (auto it = persisted.cbegin(); it != persisted.cend(); ++it) {
            if (!liveKeys.contains(it.key())) {
                m_memoryStore->deleteConnectorState(it.key());
            }
        }
        return;
    }

    if (m_storagePath.trimmed().isEmpty()) {
        return;
    }

    const QFileInfo info(m_storagePath);
    if (!info.absolutePath().trimmed().isEmpty()) {
        QDir().mkpath(info.absolutePath());
    }

    QJsonObject entries;
    for (auto it = m_entriesByKey.cbegin(); it != m_entriesByKey.cend(); ++it) {
        entries.insert(it.key(), toJson(it.value()));
    }

    QFile file(m_storagePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }

    const QJsonDocument document(QJsonObject{
        {QStringLiteral("entries"), entries}
    });
    file.write(document.toJson(QJsonDocument::Compact));
    file.close();
}

void ConnectorHistoryTracker::prune(qint64 nowMs)
{
    if (nowMs <= 0) {
        return;
    }

    bool changed = false;
    for (auto it = m_entriesByKey.begin(); it != m_entriesByKey.end();) {
        const qint64 lastTouchedAtMs = std::max(it->lastSeenAtMs, it->lastPresentedAtMs);
        if (lastTouchedAtMs > 0 && (nowMs - lastTouchedAtMs) > kRetentionWindowMs) {
            it = m_entriesByKey.erase(it);
            changed = true;
            continue;
        }
        ++it;
    }
    if (changed) {
        persist();
    }
}
