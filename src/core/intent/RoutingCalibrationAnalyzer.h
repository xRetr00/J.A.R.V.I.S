#pragma once

#include <QJsonObject>
#include <QStringList>

struct RoutingCalibrationBucket
{
    QString family;
    int count = 0;
    float meanConfidence = 0.0f;
    float meanAmbiguity = 0.0f;
    float clarificationRate = 0.0f;
    float backendRate = 0.0f;
    float localLikelySuccessRate = 0.0f;
    float backendLikelySuccessRate = 0.0f;
};

struct RoutingCalibrationSummary
{
    int total = 0;
    float meanConfidence = 0.0f;
    float meanAmbiguity = 0.0f;
    float clarificationFrequency = 0.0f;
    float backendEscalationFrequency = 0.0f;
    float localLikelySuccess = 0.0f;
    float backendLikelySuccess = 0.0f;
    QList<RoutingCalibrationBucket> buckets;
    QStringList thresholdObservations;
};

class RoutingCalibrationAnalyzer
{
public:
    QList<QJsonObject> parseRouteFinalRecords(const QStringList &lines) const;
    RoutingCalibrationSummary summarize(const QList<QJsonObject> &routeFinalRecords) const;
    QString renderThresholdObservations(const RoutingCalibrationSummary &summary) const;
};

