#include "cognition/ConnectorContextCompiler.h"

#include <algorithm>

#include <QRegularExpression>

namespace {
QString normalizedText(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral(" "));
    return value.simplified();
}

struct ConnectorAggregate {
    QString connectorKind;
    QStringList sourceKinds;
    QStringList historyKeys;
    int seenCount = 0;
    int presentedCount = 0;
    int recentSeenCount = 0;
    int recentPresentedCount = 0;
    qint64 lastSeenAtMs = 0;
};

QString connectorSummaryText(const ConnectorAggregate &aggregate)
{
    const QString connectorName = aggregate.connectorKind.isEmpty()
        ? QStringLiteral("connector")
        : aggregate.connectorKind;
    const int trackedSourceCount = std::max(aggregate.historyKeys.size(), aggregate.sourceKinds.size());
    return QStringLiteral("%1 activity across %2 sources: seen %3 times, surfaced %4 times, %5 recent source updates.")
        .arg(connectorName.left(1).toUpper() + connectorName.mid(1),
             QString::number(trackedSourceCount),
             QString::number(aggregate.seenCount),
             QString::number(aggregate.presentedCount),
             QString::number(aggregate.recentSeenCount));
}

int connectorSummaryScore(const QString &query, const ConnectorAggregate &aggregate)
{
    QStringList tokens = aggregate.sourceKinds;
    tokens.append(aggregate.historyKeys);
    tokens.push_back(aggregate.connectorKind);
    tokens.push_back(connectorSummaryText(aggregate));
    const QString haystack = tokens.join(QLatin1Char(' ')).toLower();

    int score = query.isEmpty() ? 8 : 0;
    if (!query.isEmpty() && haystack.contains(query)) {
        score += 80;
    }

    score += std::min(aggregate.seenCount, 30);
    score += std::min(aggregate.presentedCount * 2, 24);
    score += aggregate.recentSeenCount * 12;
    score += aggregate.recentPresentedCount * 8;
    if (aggregate.lastSeenAtMs > 0) {
        score += 10;
    }
    return score;
}
}

QList<MemoryRecord> ConnectorContextCompiler::compileSummaries(const QString &query,
                                                               const QHash<QString, QVariantMap> &stateByKey,
                                                               int maxCount)
{
    QHash<QString, ConnectorAggregate> aggregates;
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
        aggregate.lastSeenAtMs = std::max(aggregate.lastSeenAtMs, state.value(QStringLiteral("lastSeenAtMs")).toLongLong());

        const QString sourceKind = state.value(QStringLiteral("sourceKind")).toString().trimmed();
        if (!sourceKind.isEmpty() && !aggregate.sourceKinds.contains(sourceKind)) {
            aggregate.sourceKinds.push_back(sourceKind);
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
        summary.record.value = connectorSummaryText(aggregate);
        summary.record.confidence = 0.9f;
        summary.record.source = QStringLiteral("connector_summary");
        summary.record.updatedAt = QString::number(aggregate.lastSeenAtMs);
        summary.score = connectorSummaryScore(normalizedQuery, aggregate);
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
