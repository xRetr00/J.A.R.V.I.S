#include "cognition/CompiledContextPolicyTuningSignalBuilder.h"

#include <QDateTime>

namespace {
struct TuningProfile
{
    QString currentMode;
    QString volatilityLabel;
    int shifts = 0;
    int sustainedObservations = 0;
    int totalObservations = 0;
    double alignmentBoost = 0.06;
    double defocusPenalty = 0.05;
    double volatilityPenalty = 0.04;
    double suppressionScoreThreshold = 0.72;
    int version = 0;
    QString promotionAction;
    QString promotionReason;
    QString updatedAt;
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
        .source = QStringLiteral("compiled_history_policy_tuning"),
        .updatedAt = updatedAt
    };
}

QString modeName(const QString &mode)
{
    if (mode == QStringLiteral("research_analysis")) {
        return QStringLiteral("research analysis");
    }
    if (mode == QStringLiteral("inbox_triage")) {
        return QStringLiteral("inbox triage");
    }
    if (mode == QStringLiteral("schedule_coordination")) {
        return QStringLiteral("schedule coordination");
    }
    if (mode == QStringLiteral("document_work")) {
        return QStringLiteral("document-focused work");
    }
    return mode;
}

TuningProfile buildProfile(const QVariantList &history)
{
    TuningProfile profile;
    QString previousMode;
    QVariantMap lastState;
    for (const QVariant &entry : history) {
        const QVariantMap state = entry.toMap();
        const QString mode = state.value(QStringLiteral("dominantMode")).toString().trimmed();
        if (mode.isEmpty()) {
            continue;
        }

        if (!previousMode.isEmpty() && previousMode != mode) {
            ++profile.shifts;
        }
        previousMode = mode;
        lastState = state;
        profile.sustainedObservations = qMax(1, state.value(QStringLiteral("observations")).toInt());
        profile.totalObservations += profile.sustainedObservations;
    }

    profile.currentMode = lastState.value(QStringLiteral("dominantMode")).toString().trimmed();
    profile.updatedAt = QString::number(lastState.value(QStringLiteral("updatedAtMs"),
                                                        QDateTime::currentMSecsSinceEpoch()).toLongLong());

    const bool highVolatility = profile.shifts >= 3;
    const bool mediumVolatility = !highVolatility && profile.shifts >= 2;
    const bool strongStability = profile.sustainedObservations >= 3;
    const bool mediumStability = !strongStability && profile.sustainedObservations >= 2;

    profile.volatilityLabel = QStringLiteral("steady");
    if (highVolatility) {
        profile.volatilityLabel = QStringLiteral("elevated");
        profile.volatilityPenalty = 0.08;
        profile.suppressionScoreThreshold = 0.78;
    } else if (mediumVolatility) {
        profile.volatilityLabel = QStringLiteral("recovering");
        profile.volatilityPenalty = 0.06;
        profile.suppressionScoreThreshold = 0.75;
    }

    if (strongStability) {
        profile.alignmentBoost = 0.10;
        profile.defocusPenalty = 0.08;
    } else if (mediumStability) {
        profile.alignmentBoost = 0.08;
        profile.defocusPenalty = 0.07;
    }

    return profile;
}

TuningProfile profileFromState(const QVariantMap &state)
{
    TuningProfile profile;
    profile.currentMode = state.value(QStringLiteral("tuningCurrentMode")).toString().trimmed();
    profile.volatilityLabel = state.value(QStringLiteral("tuningVolatilityLevel")).toString().trimmed();
    profile.alignmentBoost = state.value(QStringLiteral("tuningAlignmentBoost"), 0.06).toDouble();
    profile.defocusPenalty = state.value(QStringLiteral("tuningDefocusPenalty"), 0.05).toDouble();
    profile.volatilityPenalty = state.value(QStringLiteral("tuningVolatilityPenalty"), 0.04).toDouble();
    profile.suppressionScoreThreshold =
        state.value(QStringLiteral("tuningSuppressionScoreThreshold"), 0.72).toDouble();
    profile.sustainedObservations = state.value(QStringLiteral("tuningObservedCount")).toInt();
    profile.shifts = state.value(QStringLiteral("tuningShiftCount")).toInt();
    profile.totalObservations = state.value(QStringLiteral("tuningTotalObservations"),
                                            profile.sustainedObservations).toInt();
    profile.version = state.value(QStringLiteral("version")).toInt();
    profile.promotionAction = state.value(QStringLiteral("tuningPromotionAction")).toString().trimmed();
    profile.promotionReason = state.value(QStringLiteral("tuningPromotionReason")).toString().trimmed();
    profile.updatedAt = QString::number(state.value(QStringLiteral("updatedAtMs"),
                                                    QDateTime::currentMSecsSinceEpoch()).toLongLong());
    if (profile.volatilityLabel.isEmpty()) {
        profile.volatilityLabel = QStringLiteral("steady");
    }
    return profile;
}

QList<MemoryRecord> recordsFromProfile(const TuningProfile &profile)
{
    if (profile.currentMode.isEmpty()) {
        return {};
    }

    QList<MemoryRecord> records;
    records.push_back(makeRecord(
        QStringLiteral("compiled_context_policy_tuning_signal"),
        QStringLiteral("Tuning signal: policy volatility %1 after %2 mode shifts across %3 total observations; current mode %4.")
            .arg(profile.volatilityLabel,
                 QString::number(profile.shifts),
                 QString::number(qMax(1, profile.totalObservations)),
                 profile.currentMode),
        0.84,
        profile.updatedAt));

    records.push_back(makeRecord(
        QStringLiteral("compiled_context_policy_volatility"),
        QStringLiteral("Policy volatility: %1. Use stricter confidence and stronger defocus control when medium-priority proposals conflict with current mode %2.")
            .arg(profile.volatilityLabel, profile.currentMode),
        profile.volatilityLabel == QStringLiteral("elevated") ? 0.88 : 0.8,
        profile.updatedAt));

    if (profile.sustainedObservations >= 2) {
        records.push_back(makeRecord(
            QStringLiteral("compiled_context_policy_stability_bias"),
            QStringLiteral("Policy stability bias: %1 sustained for %2 observations. Increase alignment bias for %3 and trim medium-priority defocused suggestions.")
                .arg(profile.currentMode,
                     QString::number(profile.sustainedObservations),
                     modeName(profile.currentMode)),
            profile.sustainedObservations >= 3 ? 0.89 : 0.83,
            profile.updatedAt));
    }

    records.push_back(makeRecord(
        QStringLiteral("compiled_context_policy_tuning_knobs"),
        QStringLiteral("Tuning knobs: alignmentBoost=%1 defocusPenalty=%2 volatilityPenalty=%3 suppressionScoreThreshold=%4 version=%5 action=%6.")
            .arg(QString::number(profile.alignmentBoost, 'f', 2),
                 QString::number(profile.defocusPenalty, 'f', 2),
                 QString::number(profile.volatilityPenalty, 'f', 2),
                 QString::number(profile.suppressionScoreThreshold, 'f', 2),
                 QString::number(profile.version),
                 profile.promotionAction.isEmpty() ? QStringLiteral("derived") : profile.promotionAction),
        0.82,
        profile.updatedAt));

    return records;
}

QVariantMap plannerMetadataFromProfile(const TuningProfile &profile)
{
    if (profile.currentMode.isEmpty()) {
        return {};
    }

    return {
        {QStringLiteral("tuningCurrentMode"), profile.currentMode},
        {QStringLiteral("tuningVolatilityLevel"), profile.volatilityLabel},
        {QStringLiteral("tuningAlignmentBoost"), profile.alignmentBoost},
        {QStringLiteral("tuningDefocusPenalty"), profile.defocusPenalty},
        {QStringLiteral("tuningVolatilityPenalty"), profile.volatilityPenalty},
        {QStringLiteral("tuningSuppressionScoreThreshold"), profile.suppressionScoreThreshold},
        {QStringLiteral("tuningObservedCount"), profile.sustainedObservations},
        {QStringLiteral("tuningShiftCount"), profile.shifts},
        {QStringLiteral("tuningTotalObservations"), profile.totalObservations},
        {QStringLiteral("tuningVersion"), profile.version},
        {QStringLiteral("tuningPromotionAction"), profile.promotionAction},
        {QStringLiteral("tuningPromotionReason"), profile.promotionReason}
    };
}
}

QList<MemoryRecord> CompiledContextPolicyTuningSignalBuilder::build(const QVariantList &history)
{
    if (history.isEmpty()) {
        return {};
    }

    const TuningProfile profile = buildProfile(history);
    return recordsFromProfile(profile);
}

QList<MemoryRecord> CompiledContextPolicyTuningSignalBuilder::buildFromState(const QVariantMap &state)
{
    return recordsFromProfile(profileFromState(state));
}

QVariantMap CompiledContextPolicyTuningSignalBuilder::buildState(const QVariantList &history)
{
    const TuningProfile profile = buildProfile(history);
    QVariantMap state = plannerMetadataFromProfile(profile);
    if (state.isEmpty()) {
        return {};
    }
    state.insert(QStringLiteral("updatedAtMs"), profile.updatedAt.toLongLong());
    return state;
}

QVariantMap CompiledContextPolicyTuningSignalBuilder::buildPlannerMetadata(const QVariantList &history)
{
    return plannerMetadataFromProfile(buildProfile(history));
}

QVariantMap CompiledContextPolicyTuningSignalBuilder::buildPlannerMetadataFromState(const QVariantMap &state)
{
    return plannerMetadataFromProfile(profileFromState(state));
}
