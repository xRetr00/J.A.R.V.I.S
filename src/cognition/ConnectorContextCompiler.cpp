#include "cognition/ConnectorContextCompiler.h"

#include <algorithm>

#include <QDateTime>
#include <QRegularExpression>

namespace {
QString normalizedText(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral(" "));
    return value.simplified();
}

QString friendlySourceName(const QString &sourceKind)
{
    if (sourceKind == QStringLiteral("connector_schedule_calendar")) {
        return QStringLiteral("calendar");
    }
    if (sourceKind == QStringLiteral("connector_inbox_maildrop")) {
        return QStringLiteral("maildrop");
    }
    if (sourceKind == QStringLiteral("connector_notes_directory")) {
        return QStringLiteral("notes directory");
    }
    if (sourceKind == QStringLiteral("connector_research_browser")) {
        return QStringLiteral("browser research");
    }
    return sourceKind.trimmed().isEmpty() ? QStringLiteral("connector source") : sourceKind.trimmed();
}

int sourcePriorityWeight(const QString &sourceKind)
{
    if (sourceKind == QStringLiteral("connector_schedule_calendar")) {
        return 36;
    }
    if (sourceKind == QStringLiteral("connector_inbox_maildrop")) {
        return 34;
    }
    if (sourceKind == QStringLiteral("connector_notes_directory")) {
        return 24;
    }
    if (sourceKind == QStringLiteral("connector_research_browser")) {
        return 18;
    }
    return 10;
}

int freshnessScore(qint64 lastSeenAtMs, qint64 nowMs)
{
    if (lastSeenAtMs <= 0 || nowMs <= 0 || lastSeenAtMs > nowMs) {
        return 0;
    }

    const qint64 ageMs = nowMs - lastSeenAtMs;
    if (ageMs <= 10LL * 60LL * 1000LL) {
        return 28;
    }
    if (ageMs <= 60LL * 60LL * 1000LL) {
        return 20;
    }
    if (ageMs <= 24LL * 60LL * 60LL * 1000LL) {
        return 10;
    }
    if (ageMs <= 7LL * 24LL * 60LL * 60LL * 1000LL) {
        return 2;
    }
    return -12;
}

QString freshnessLabel(qint64 lastSeenAtMs, qint64 nowMs)
{
    if (lastSeenAtMs <= 0 || nowMs <= 0 || lastSeenAtMs > nowMs) {
        return QStringLiteral("unknown freshness");
    }

    const qint64 ageMs = nowMs - lastSeenAtMs;
    if (ageMs <= 10LL * 60LL * 1000LL) {
        return QStringLiteral("updated very recently");
    }
    if (ageMs <= 60LL * 60LL * 1000LL) {
        return QStringLiteral("updated within the last hour");
    }
    if (ageMs <= 24LL * 60LL * 60LL * 1000LL) {
        return QStringLiteral("updated today");
    }
    if (ageMs <= 7LL * 24LL * 60LL * 60LL * 1000LL) {
        return QStringLiteral("updated this week");
    }
    return QStringLiteral("stale");
}

struct ConnectorAggregate {
    QString connectorKind;
    QStringList sourceKinds;
    QStringList historyKeys;
    QString dominantSourceKind;
    int seenCount = 0;
    int presentedCount = 0;
    int recentSeenCount = 0;
    int recentPresentedCount = 0;
    int sourcePriority = 0;
    qint64 firstSeenAtMs = 0;
    qint64 lastSeenAtMs = 0;
    qint64 lastPresentedAtMs = 0;
};

QString connectorSummaryText(const ConnectorAggregate &aggregate, qint64 nowMs)
{
    const QString connectorName = aggregate.connectorKind.isEmpty()
        ? QStringLiteral("connector")
        : aggregate.connectorKind;
    const int trackedSourceCount = std::max(aggregate.historyKeys.size(), aggregate.sourceKinds.size());
    const qint64 trackingWindowDays = (aggregate.firstSeenAtMs > 0 && aggregate.lastSeenAtMs >= aggregate.firstSeenAtMs)
        ? std::max<qint64>(1, ((aggregate.lastSeenAtMs - aggregate.firstSeenAtMs) / (24LL * 60LL * 60LL * 1000LL)) + 1)
        : 1;
    return QStringLiteral("%1 activity across %2 sources: seen %3 times, surfaced %4 times, top source %5, tracked %6 days, %7.")
        .arg(connectorName.left(1).toUpper() + connectorName.mid(1),
             QString::number(trackedSourceCount),
             QString::number(aggregate.seenCount),
             QString::number(aggregate.presentedCount),
             friendlySourceName(aggregate.dominantSourceKind),
             QString::number(trackingWindowDays),
             freshnessLabel(aggregate.lastSeenAtMs, nowMs));
}

int connectorSummaryScore(const QString &query, const ConnectorAggregate &aggregate, qint64 nowMs)
{
    QStringList tokens = aggregate.sourceKinds;
    tokens.append(aggregate.historyKeys);
    tokens.push_back(aggregate.connectorKind);
    tokens.push_back(connectorSummaryText(aggregate, nowMs));
    const QString haystack = tokens.join(QLatin1Char(' ')).toLower();

    int score = query.isEmpty() ? 8 : 0;
    if (!query.isEmpty() && haystack.contains(query)) {
        score += 80;
    }

    score += std::min(aggregate.seenCount, 30);
    score += std::min(aggregate.presentedCount * 2, 24);
    score += aggregate.recentSeenCount * 12;
    score += aggregate.recentPresentedCount * 8;
    score += aggregate.sourcePriority;
    score += freshnessScore(aggregate.lastSeenAtMs, nowMs);
    return score;
}
}

QList<MemoryRecord> ConnectorContextCompiler::compileSummaries(const QString &query,
                                                               const QHash<QString, QVariantMap> &stateByKey,
                                                               int maxCount)
{
    QHash<QString, ConnectorAggregate> aggregates;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto it = stateByKey.cbegin(); it != stateByKey.cend(); ++it) {
        const QVariantMap &state = it.value();
        const QString connectorKind = state.value(QStringLiteral("connectorKind")).toString().trimmed().toLower();
        if (connectorKind.isEmpty()) {
            continue;
        }

        ConnectorAggregate &aggregate = aggregates[connectorKind];
        aggregate.connectorKind = connectorKind;
        aggregate.seenCount += state.value(QStringLiteral("seenCount")).toInt();
        aggregate.presentedCount += state.value(QStringLiteral("presentedCount")).toInt();
        aggregate.recentSeenCount += state.value(QStringLiteral("historyRecentlySeen")).toBool() ? 1 : 0;
        aggregate.recentPresentedCount += state.value(QStringLiteral("historyRecentlyPresented")).toBool() ? 1 : 0;
        const qint64 firstSeenAtMs = state.value(QStringLiteral("firstSeenAtMs")).toLongLong();
        if (firstSeenAtMs > 0 && (aggregate.firstSeenAtMs <= 0 || firstSeenAtMs < aggregate.firstSeenAtMs)) {
            aggregate.firstSeenAtMs = firstSeenAtMs;
        }
        aggregate.lastSeenAtMs = std::max(aggregate.lastSeenAtMs, state.value(QStringLiteral("lastSeenAtMs")).toLongLong());
        aggregate.lastPresentedAtMs = std::max(aggregate.lastPresentedAtMs, state.value(QStringLiteral("lastPresentedAtMs")).toLongLong());

        const QString sourceKind = state.value(QStringLiteral("sourceKind")).toString().trimmed();
        if (!sourceKind.isEmpty() && !aggregate.sourceKinds.contains(sourceKind)) {
            aggregate.sourceKinds.push_back(sourceKind);
        }
        const int sourcePriority = sourcePriorityWeight(sourceKind);
        if (sourcePriority > aggregate.sourcePriority) {
            aggregate.sourcePriority = sourcePriority;
            aggregate.dominantSourceKind = sourceKind;
        }

        const QString historyKey = state.value(QStringLiteral("historyKey")).toString().trimmed().isEmpty()
            ? it.key()
            : state.value(QStringLiteral("historyKey")).toString().trimmed();
        if (!historyKey.isEmpty() && !aggregate.historyKeys.contains(historyKey)) {
            aggregate.historyKeys.push_back(historyKey);
        }
    }

    struct RankedSummary {
        MemoryRecord record;
        int score = 0;
    };

    QList<RankedSummary> ranked;
    const QString normalizedQuery = normalizedText(query);
    for (auto it = aggregates.cbegin(); it != aggregates.cend(); ++it) {
        const ConnectorAggregate &aggregate = it.value();
        RankedSummary summary;
        summary.record.type = QStringLiteral("context");
        summary.record.key = QStringLiteral("connector_summary_%1").arg(aggregate.connectorKind);
        summary.record.value = connectorSummaryText(aggregate, nowMs);
        summary.record.confidence = 0.9f;
        summary.record.source = QStringLiteral("connector_summary");
        summary.record.updatedAt = QString::number(aggregate.lastSeenAtMs);
        summary.score = connectorSummaryScore(normalizedQuery, aggregate, nowMs);
        if (summary.score > 0) {
            ranked.push_back(summary);
        }
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedSummary &left, const RankedSummary &right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.record.updatedAt > right.record.updatedAt;
    });

    QList<MemoryRecord> records;
    for (const RankedSummary &summary : ranked) {
        records.push_back(summary.record);
        if (records.size() >= maxCount) {
            break;
        }
    }
    return records;
}
