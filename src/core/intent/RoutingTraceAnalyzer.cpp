#include "core/intent/RoutingTraceAnalyzer.h"

#include <algorithm>

RoutingTraceAnalyzerSummary RoutingTraceAnalyzer::summarize(const QList<RoutingTrace> &traces) const
{
    RoutingTraceAnalyzerSummary summary;
    summary.total = traces.size();
    QHash<QString, int> patternCounts;

    for (const RoutingTrace &trace : traces) {
        const bool misroute = trace.finalDecision.kind == InputRouteKind::None
            || trace.finalExecutedRoute == QStringLiteral("none");
        if (misroute) {
            summary.misroutes++;
            patternCounts[QStringLiteral("misroute.none")] += 1;
        }
        if (!trace.overridesApplied.isEmpty()) {
            summary.overrideCount += trace.overridesApplied.size();
            patternCounts[QStringLiteral("overrides.present")] += 1;
        }
        if (trace.ambiguityScore >= 0.55f) {
            summary.highAmbiguityCount++;
            patternCounts[QStringLiteral("ambiguity.high")] += 1;
        }
        for (const QString &overrideCode : trace.overridesApplied) {
            if (overrideCode.contains(QStringLiteral("fallback"), Qt::CaseInsensitive)) {
                summary.fallbackCount++;
                patternCounts[QStringLiteral("fallback.used")] += 1;
                break;
            }
        }
    }

    QList<QPair<QString, int>> sorted;
    sorted.reserve(patternCounts.size());
    for (auto it = patternCounts.begin(); it != patternCounts.end(); ++it) {
        sorted.push_back(qMakePair(it.key(), it.value()));
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &left, const auto &right) {
        return left.second > right.second;
    });
    const int topCount = std::min(3, static_cast<int>(sorted.size()));
    for (int i = 0; i < topCount; ++i) {
        summary.topProblemPatterns.push_back(
            QStringLiteral("%1:%2").arg(sorted.at(i).first).arg(sorted.at(i).second));
    }

    return summary;
}
