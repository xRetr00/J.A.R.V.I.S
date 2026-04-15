#include "cognition/CompiledContextPolicySummaryBuilder.h"

#include <QDateTime>

#include "cognition/CompiledContextHistoryPolicy.h"

namespace {
QString modeFocusText(const QString &mode)
{
    if (mode == QStringLiteral("document_work")) {
        return QStringLiteral("Prioritize document, workspace, and file-grounded continuity.");
    }
    if (mode == QStringLiteral("schedule_coordination")) {
        return QStringLiteral("Prioritize calendar, deadline, and meeting-aware continuity.");
    }
    if (mode == QStringLiteral("inbox_triage")) {
        return QStringLiteral("Prioritize message review, summarization, and reply preparation.");
    }
    if (mode == QStringLiteral("research_analysis")) {
        return QStringLiteral("Prioritize source review, synthesis, and evidence-grounded browsing.");
    }
    return {};
}

QString connectorSourceSummary(const QString &connectorKind,
                               const QHash<QString, QVariantMap> &connectorStateMap)
{
    int totalSeen = 0;
    int totalPresented = 0;
    QStringList sourceKinds;
    for (auto it = connectorStateMap.cbegin(); it != connectorStateMap.cend(); ++it) {
        const QVariantMap state = it.value();
        if (state.value(QStringLiteral("connectorKind")).toString().trimmed().toLower() != connectorKind) {
            continue;
        }
        totalSeen += state.value(QStringLiteral("seenCount")).toInt();
        totalPresented += state.value(QStringLiteral("presentedCount")).toInt();
        const QString sourceKind = state.value(QStringLiteral("sourceKind")).toString().trimmed();
        if (!sourceKind.isEmpty() && !sourceKinds.contains(sourceKind)) {
            sourceKinds.push_back(sourceKind);
        }
    }

    if (totalSeen <= 0 && sourceKinds.isEmpty()) {
        return {};
    }

    return QStringLiteral("%1 source continuity: seen %2 times, surfaced %3 times, sources %4.")
        .arg(connectorKind,
             QString::number(totalSeen),
             QString::number(totalPresented),
             sourceKinds.isEmpty() ? QStringLiteral("none") : sourceKinds.join(QStringLiteral(", ")));
}

QString sourceSummaryForMode(const QString &mode,
                             const QHash<QString, QVariantMap> &connectorStateMap)
{
    if (mode == QStringLiteral("schedule_coordination")) {
        return connectorSourceSummary(QStringLiteral("schedule"), connectorStateMap);
    }
    if (mode == QStringLiteral("inbox_triage")) {
        return connectorSourceSummary(QStringLiteral("inbox"), connectorStateMap);
    }
    if (mode == QStringLiteral("research_analysis")) {
        return connectorSourceSummary(QStringLiteral("research"), connectorStateMap);
    }
    if (mode == QStringLiteral("document_work")) {
        return QStringLiteral("Desktop document and workspace continuity remains the dominant long-horizon source.");
    }
    return {};
}

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
        .source = QStringLiteral("compiled_history_policy_summary"),
        .updatedAt = updatedAt
    };
}
}

QList<MemoryRecord> CompiledContextPolicySummaryBuilder::build(const QVariantMap &policyState,
                                                               const QHash<QString, QVariantMap> &connectorStateMap)
{
    const CompiledContextHistoryPolicyDecision decision =
        CompiledContextHistoryPolicy::fromState(policyState);
    if (!decision.isValid()) {
        return {};
    }

    const QString updatedAt = QString::number(policyState.value(QStringLiteral("updatedAtMs"),
                                                                QDateTime::currentMSecsSinceEpoch()).toLongLong());
    QList<MemoryRecord> records;
    records.push_back(makeRecord(
        QStringLiteral("compiled_context_policy_summary"),
        QStringLiteral("Long-horizon policy mode %1. %2")
            .arg(decision.dominantMode, decision.selectionDirective),
        0.9,
        updatedAt));

    const QString focusText = modeFocusText(decision.dominantMode);
    if (!focusText.isEmpty()) {
        records.push_back(makeRecord(
            QStringLiteral("compiled_context_policy_focus"),
            focusText,
            0.86,
            updatedAt));
    }

    const QString sourceText = sourceSummaryForMode(decision.dominantMode, connectorStateMap);
    if (!sourceText.isEmpty()) {
        records.push_back(makeRecord(
            QStringLiteral("compiled_context_policy_sources"),
            sourceText,
            0.82,
            updatedAt));
    }

    return records;
}
