#include "core/intent/RoutingCalibrationAnalyzer.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>

#include "core/intent/IntentTuningConfig.h"

namespace {
QString routeFamily(const QString &finalRoute)
{
    const QString lowered = finalRoute.trimmed().toLower();
    if (lowered == QStringLiteral("local_response")
        || lowered == QStringLiteral("deterministic_tasks")
        || lowered == QStringLiteral("background_tasks")) {
        return QStringLiteral("local_family");
    }
    if (lowered == QStringLiteral("conversation")
        || lowered == QStringLiteral("agent_conversation")
        || lowered == QStringLiteral("command_extraction")) {
        return QStringLiteral("backend_family");
    }
    if (lowered == QStringLiteral("pending_confirmation")) {
        return QStringLiteral("safety_family");
    }
    return QStringLiteral("other_family");
}

bool hasFallbackOverride(const QJsonObject &record)
{
    const QJsonArray overrides = record.value(QStringLiteral("overrides")).toArray();
    for (const QJsonValue &value : overrides) {
        const QString code = value.toString();
        if (code.contains(QStringLiteral("fallback"), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
}

QList<QJsonObject> RoutingCalibrationAnalyzer::parseRouteFinalRecords(const QStringList &lines) const
{
    QList<QJsonObject> records;
    for (const QString &line : lines) {
        const int marker = line.indexOf(QStringLiteral("[route_final]"));
        const QString payload = marker >= 0
            ? line.mid(marker + QStringLiteral("[route_final]").size()).trimmed()
            : line.trimmed();
        if (payload.isEmpty()) {
            continue;
        }
        const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8());
        if (!document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("record")).toString() == QStringLiteral("route_final")) {
            records.push_back(object);
        }
    }
    return records;
}

RoutingCalibrationSummary RoutingCalibrationAnalyzer::summarize(const QList<QJsonObject> &routeFinalRecords) const
{
    RoutingCalibrationSummary summary;
    summary.total = routeFinalRecords.size();
    if (summary.total == 0) {
        return summary;
    }

    QHash<QString, QList<QJsonObject>> byFamily;
    int clarificationCount = 0;
    int backendCount = 0;
    int localLikelySuccessCount = 0;
    int backendLikelySuccessCount = 0;
    float confidenceSum = 0.0f;
    float ambiguitySum = 0.0f;

    for (const QJsonObject &record : routeFinalRecords) {
        const QString finalRoute = record.value(QStringLiteral("final_executed_route")).toString();
        const QString family = routeFamily(finalRoute);
        byFamily[family].push_back(record);

        const float finalConfidence = static_cast<float>(record.value(QStringLiteral("intent_confidence"))
                                                             .toObject()
                                                             .value(QStringLiteral("final"))
                                                             .toDouble());
        const float ambiguity = static_cast<float>(record.value(QStringLiteral("ambiguity_score")).toDouble());
        confidenceSum += finalConfidence;
        ambiguitySum += ambiguity;

        const bool isClarification = finalRoute == QStringLiteral("local_response")
            && record.value(QStringLiteral("arbitrator_reason_codes")).toArray().contains(
                QJsonValue(QStringLiteral("arbitrator.ask_clarification")));
        if (isClarification) {
            clarificationCount++;
        }

        const bool isBackend = family == QStringLiteral("backend_family");
        if (isBackend) {
            backendCount++;
        }

        const bool fallback = hasFallbackOverride(record);
        if (family == QStringLiteral("local_family") && !fallback) {
            localLikelySuccessCount++;
        }
        if (family == QStringLiteral("backend_family") && !fallback) {
            backendLikelySuccessCount++;
        }
    }

    summary.meanConfidence = confidenceSum / static_cast<float>(summary.total);
    summary.meanAmbiguity = ambiguitySum / static_cast<float>(summary.total);
    summary.clarificationFrequency = static_cast<float>(clarificationCount) / static_cast<float>(summary.total);
    summary.backendEscalationFrequency = static_cast<float>(backendCount) / static_cast<float>(summary.total);
    summary.localLikelySuccess = static_cast<float>(localLikelySuccessCount) / static_cast<float>(summary.total);
    summary.backendLikelySuccess = static_cast<float>(backendLikelySuccessCount) / static_cast<float>(summary.total);

    for (auto it = byFamily.begin(); it != byFamily.end(); ++it) {
        RoutingCalibrationBucket bucket;
        bucket.family = it.key();
        bucket.count = it.value().size();
        float bucketConfidence = 0.0f;
        float bucketAmbiguity = 0.0f;
        int bucketClarify = 0;
        int bucketBackend = 0;
        int localBucketSuccess = 0;
        int backendBucketSuccess = 0;
        for (const QJsonObject &record : it.value()) {
            const QString finalRoute = record.value(QStringLiteral("final_executed_route")).toString();
            bucketConfidence += static_cast<float>(record.value(QStringLiteral("intent_confidence"))
                                                       .toObject()
                                                       .value(QStringLiteral("final"))
                                                       .toDouble());
            bucketAmbiguity += static_cast<float>(record.value(QStringLiteral("ambiguity_score")).toDouble());
            const bool fallback = hasFallbackOverride(record);
            if (finalRoute == QStringLiteral("local_response")
                && record.value(QStringLiteral("arbitrator_reason_codes")).toArray().contains(
                    QJsonValue(QStringLiteral("arbitrator.ask_clarification")))) {
                bucketClarify++;
            }
            if (routeFamily(finalRoute) == QStringLiteral("backend_family")) {
                bucketBackend++;
            }
            if (routeFamily(finalRoute) == QStringLiteral("local_family") && !fallback) {
                localBucketSuccess++;
            }
            if (routeFamily(finalRoute) == QStringLiteral("backend_family") && !fallback) {
                backendBucketSuccess++;
            }
        }
        if (bucket.count > 0) {
            const float denominator = static_cast<float>(bucket.count);
            bucket.meanConfidence = bucketConfidence / denominator;
            bucket.meanAmbiguity = bucketAmbiguity / denominator;
            bucket.clarificationRate = static_cast<float>(bucketClarify) / denominator;
            bucket.backendRate = static_cast<float>(bucketBackend) / denominator;
            bucket.localLikelySuccessRate = static_cast<float>(localBucketSuccess) / denominator;
            bucket.backendLikelySuccessRate = static_cast<float>(backendBucketSuccess) / denominator;
        }
        summary.buckets.push_back(bucket);
    }

    summary.thresholdObservations = renderThresholdObservations(summary).split(QChar::fromLatin1('\n'));
    summary.thresholdObservations.erase(
        std::remove_if(summary.thresholdObservations.begin(),
                       summary.thresholdObservations.end(),
                       [](const QString &line) { return line.trimmed().isEmpty(); }),
        summary.thresholdObservations.end());
    return summary;
}

QString RoutingCalibrationAnalyzer::renderThresholdObservations(const RoutingCalibrationSummary &summary) const
{
    const IntentTuningThresholds &thresholds = IntentTuningConfig::thresholds();
    QStringList lines;
    lines.push_back(
        QStringLiteral("mean_confidence=%1 mean_ambiguity=%2")
            .arg(summary.meanConfidence, 0, 'f', 3)
            .arg(summary.meanAmbiguity, 0, 'f', 3));

    if (summary.clarificationFrequency > 0.25f) {
        lines.push_back(
            QStringLiteral("observe: clarification high (%1). consider raising ambiguity threshold above %2")
                .arg(summary.clarificationFrequency, 0, 'f', 3)
                .arg(thresholds.highAmbiguity, 0, 'f', 2));
    }
    if (summary.backendEscalationFrequency > 0.65f) {
        lines.push_back(
            QStringLiteral("observe: backend escalation high (%1). consider raising backend assist threshold above %2")
                .arg(summary.backendEscalationFrequency, 0, 'f', 3)
                .arg(thresholds.backendAssistNeed, 0, 'f', 2));
    }
    if (summary.meanConfidence < thresholds.mediumConfidence) {
        lines.push_back(
            QStringLiteral("observe: mean confidence below medium (%1 < %2). review confidence scoring weights")
                .arg(summary.meanConfidence, 0, 'f', 3)
                .arg(thresholds.mediumConfidence, 0, 'f', 2));
    }
    if (summary.localLikelySuccess + 0.15f < summary.backendLikelySuccess) {
        lines.push_back(QStringLiteral("observe: backend likely-success dominates local; review local candidate confidence penalties"));
    }
    if (summary.localLikelySuccess > summary.backendLikelySuccess + 0.2f) {
        lines.push_back(QStringLiteral("observe: local likely-success dominates backend; review backend escalation aggressiveness"));
    }

    if (lines.size() == 1) {
        lines.push_back(QStringLiteral("observe: thresholds appear balanced in this sample"));
    }
    return lines.join(QChar::fromLatin1('\n'));
}

