#include "cognition/CompiledContextLayeredMemoryBuilder.h"

#include <QDateTime>

#include "cognition/CompiledContextHistoryPolicy.h"

namespace {
struct ConnectorAggregate
{
    QString connectorKind;
    QStringList sourceKinds;
    int seenCount = 0;
    int presentedCount = 0;
};

MemoryRecord makeRecord(const QString &key,
                        const QString &value,
                        double confidence,
                        const QString &updatedAt)
{
    return MemoryRecord{
        .type = QStringLiteral("context"),
        .key = key,
        .value = value.simplified(),
        .confidence = static_cast<float>(confidence),
        .source = QStringLiteral("compiled_layered_memory"),
        .updatedAt = updatedAt
    };
}

QHash<QString, ConnectorAggregate> aggregateConnectors(const QHash<QString, QVariantMap> &connectorStateMap)
{
    QHash<QString, ConnectorAggregate> aggregates;
    for (auto it = connectorStateMap.cbegin(); it != connectorStateMap.cend(); ++it) {
        const QVariantMap state = it.value();
        const QString connectorKind = state.value(QStringLiteral("connectorKind")).toString().trimmed().toLower();
        if (connectorKind.isEmpty()) {
            continue;
        }

        ConnectorAggregate &aggregate = aggregates[connectorKind];
        aggregate.connectorKind = connectorKind;
        aggregate.seenCount += state.value(QStringLiteral("seenCount")).toInt();
        aggregate.presentedCount += state.value(QStringLiteral("presentedCount")).toInt();

        const QString sourceKind = state.value(QStringLiteral("sourceKind")).toString().trimmed();
        if (!sourceKind.isEmpty() && !aggregate.sourceKinds.contains(sourceKind)) {
            aggregate.sourceKinds.push_back(sourceKind);
        }
    }
    return aggregates;
}

QString continuityLine(const ConnectorAggregate &aggregate)
{
    return QStringLiteral("%1: seen %2 times, surfaced %3 times, sources %4.")
        .arg(aggregate.connectorKind,
             QString::number(aggregate.seenCount),
             QString::number(aggregate.presentedCount),
             aggregate.sourceKinds.isEmpty()
                 ? QStringLiteral("none")
                 : aggregate.sourceKinds.join(QStringLiteral(", ")));
}
}

QList<MemoryRecord> CompiledContextLayeredMemoryBuilder::build(
    const QVariantMap &policyState,
    const QHash<QString, QVariantMap> &connectorStateMap)
{
    const CompiledContextHistoryPolicyDecision decision =
        CompiledContextHistoryPolicy::fromState(policyState);
    if (!decision.isValid()) {
        return {};
    }

    const QString updatedAt = QString::number(policyState.value(QStringLiteral("updatedAtMs"),
                                                                QDateTime::currentMSecsSinceEpoch()).toLongLong());
    const QHash<QString, ConnectorAggregate> aggregates = aggregateConnectors(connectorStateMap);

    QStringList continuityLines;
    ConnectorAggregate dominantSource;
    for (auto it = aggregates.cbegin(); it != aggregates.cend(); ++it) {
        continuityLines.push_back(continuityLine(it.value()));
        if (it.value().seenCount > dominantSource.seenCount) {
            dominantSource = it.value();
        }
    }

    QList<MemoryRecord> records;
    records.push_back(makeRecord(
        QStringLiteral("compiled_context_layered_summary"),
        QStringLiteral("Layered context synthesis: dominant mode %1. %2")
            .arg(decision.dominantMode, decision.selectionDirective),
        0.88,
        updatedAt));

    records.push_back(makeRecord(
        QStringLiteral("compiled_context_layered_focus"),
        QStringLiteral("Prompt steering: %1")
            .arg(decision.promptDirective.isEmpty()
                     ? decision.selectionDirective
                     : decision.promptDirective),
        0.84,
        updatedAt));

    if (dominantSource.seenCount > 0 || !continuityLines.isEmpty()) {
        records.push_back(makeRecord(
            QStringLiteral("compiled_context_layered_continuity"),
            QStringLiteral("Dominant continuity source %1. %2")
                .arg(dominantSource.connectorKind.isEmpty()
                         ? QStringLiteral("none")
                         : continuityLine(dominantSource),
                     continuityLines.join(QStringLiteral(" "))),
            0.82,
            updatedAt));
    }

    return records;
}
