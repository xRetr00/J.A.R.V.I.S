#include "cognition/CompiledContextHistoryPolicy.h"

#include <algorithm>

#include <QDateTime>

namespace {
struct ModeScoreRow {
    QString mode;
    double score = 0.0;
};

QString normalizedRecordText(const MemoryRecord &record)
{
    return QStringLiteral("%1 %2 %3")
        .arg(record.key, record.value, record.source)
        .toLower()
        .simplified();
}

double durationWeight(const MemoryRecord &record)
{
    bool ok = false;
    const qint64 durationMs = record.updatedAt.toLongLong(&ok);
    if (!ok || durationMs <= 0) {
        return 1.0;
    }
    if (durationMs >= 2 * 60 * 60 * 1000) {
        return 1.30;
    }
    if (durationMs >= 20 * 60 * 1000) {
        return 1.15;
    }
    return 1.0;
}

void addScore(QHash<QString, double> &scores, const QString &mode, double amount)
{
    scores[mode] = scores.value(mode) + amount;
}

void scoreRecord(QHash<QString, double> &scores, const MemoryRecord &record)
{
    const QString text = normalizedRecordText(record);
    const double weight = durationWeight(record);

    if (text.contains(QStringLiteral("desktop_context_document"))
        || text.contains(QStringLiteral("editor_document"))
        || text.contains(QStringLiteral("write_file"))
        || text.contains(QStringLiteral("read_file"))
        || text.contains(QStringLiteral("workspace"))) {
        addScore(scores, QStringLiteral("document_work"), 1.4 * weight);
    }

    if (text.contains(QStringLiteral("connector_summary_schedule"))
        || text.contains(QStringLiteral(" meeting"))
        || text.contains(QStringLiteral(" calendar"))
        || text.contains(QStringLiteral("schedule"))) {
        addScore(scores, QStringLiteral("schedule_coordination"), 1.6 * weight);
    }

    if (text.contains(QStringLiteral("connector_summary_inbox"))
        || text.contains(QStringLiteral(" inbox"))
        || text.contains(QStringLiteral(" mail"))
        || text.contains(QStringLiteral(" message"))) {
        addScore(scores, QStringLiteral("inbox_triage"), 1.6 * weight);
    }

    if (text.contains(QStringLiteral("connector_summary_research"))
        || text.contains(QStringLiteral(" source"))
        || text.contains(QStringLiteral("browser_tab"))
        || text.contains(QStringLiteral("research"))) {
        addScore(scores, QStringLiteral("research_analysis"), 1.5 * weight);
    }
}

CompiledContextHistoryPolicyDecision buildDecision(const QString &mode, double score)
{
    CompiledContextHistoryPolicyDecision decision;
    decision.dominantMode = mode;
    decision.strength = score;

    if (mode == QStringLiteral("document_work")) {
        decision.selectionDirective = QStringLiteral(
            "History policy: stable document-focused work is ongoing. Prefer document, workspace, and file-grounded tools before unrelated follow-ups.");
        decision.promptDirective = QStringLiteral(
            "Stable mode: document-focused work remains active.");
        decision.reasonCode = QStringLiteral("compiled_history_policy.document_work");
    } else if (mode == QStringLiteral("schedule_coordination")) {
        decision.selectionDirective = QStringLiteral(
            "History policy: stable schedule coordination is ongoing. Prefer calendar, deadline, and meeting-aware actions before unrelated suggestions.");
        decision.promptDirective = QStringLiteral(
            "Stable mode: schedule coordination remains active.");
        decision.reasonCode = QStringLiteral("compiled_history_policy.schedule_coordination");
    } else if (mode == QStringLiteral("inbox_triage")) {
        decision.selectionDirective = QStringLiteral(
            "History policy: stable inbox triage is ongoing. Prefer message review, summarization, and reply-prep actions before unrelated follow-ups.");
        decision.promptDirective = QStringLiteral(
            "Stable mode: inbox triage remains active.");
        decision.reasonCode = QStringLiteral("compiled_history_policy.inbox_triage");
    } else if (mode == QStringLiteral("research_analysis")) {
        decision.selectionDirective = QStringLiteral(
            "History policy: stable research analysis is ongoing. Prefer source review, browsing, and evidence-grounded follow-ups before unrelated actions.");
        decision.promptDirective = QStringLiteral(
            "Stable mode: research analysis remains active.");
        decision.reasonCode = QStringLiteral("compiled_history_policy.research_analysis");
    }

    decision.plannerMetadata.insert(QStringLiteral("compiledContextHistoryMode"), decision.dominantMode);
    decision.plannerMetadata.insert(QStringLiteral("compiledContextHistoryModeStrength"), decision.strength);
    decision.plannerMetadata.insert(QStringLiteral("compiledContextHistoryDirective"), decision.selectionDirective);
    return decision;
}
}

CompiledContextHistoryPolicyDecision CompiledContextHistoryPolicy::evaluate(const QList<MemoryRecord> &records)
{
    QHash<QString, double> scores;
    for (const MemoryRecord &record : records) {
        scoreRecord(scores, record);
    }

    QList<ModeScoreRow> rows;
    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it) {
        rows.push_back(ModeScoreRow{it.key(), it.value()});
    }
    std::sort(rows.begin(), rows.end(), [](const ModeScoreRow &left, const ModeScoreRow &right) {
        return left.score > right.score;
    });

    if (rows.isEmpty() || rows.first().score < 1.5) {
        return {};
    }
    return buildDecision(rows.first().mode, rows.first().score);
}

MemoryRecord CompiledContextHistoryPolicy::buildContextRecord(const CompiledContextHistoryPolicyDecision &decision)
{
    if (!decision.isValid()) {
        return {};
    }

    return MemoryRecord{
        .type = QStringLiteral("context"),
        .key = QStringLiteral("compiled_context_history_mode"),
        .value = QStringLiteral("%1 %2")
                     .arg(decision.dominantMode, decision.selectionDirective)
                     .simplified(),
        .confidence = static_cast<float>(std::clamp(decision.strength / 4.0, 0.72, 0.94)),
        .source = QStringLiteral("compiled_history_policy"),
        .updatedAt = QString::number(QDateTime::currentMSecsSinceEpoch())
    };
}

QVariantMap CompiledContextHistoryPolicy::buildState(const CompiledContextHistoryPolicyDecision &decision)
{
    if (!decision.isValid()) {
        return {};
    }

    QVariantMap state = decision.plannerMetadata;
    state.insert(QStringLiteral("dominantMode"), decision.dominantMode);
    state.insert(QStringLiteral("selectionDirective"), decision.selectionDirective);
    state.insert(QStringLiteral("promptDirective"), decision.promptDirective);
    state.insert(QStringLiteral("reasonCode"), decision.reasonCode);
    state.insert(QStringLiteral("strength"), decision.strength);
    state.insert(QStringLiteral("updatedAtMs"), QDateTime::currentMSecsSinceEpoch());
    return state;
}

CompiledContextHistoryPolicyDecision CompiledContextHistoryPolicy::fromState(const QVariantMap &state)
{
    CompiledContextHistoryPolicyDecision decision;
    decision.dominantMode = state.value(QStringLiteral("dominantMode")).toString().trimmed();
    decision.selectionDirective = state.value(QStringLiteral("selectionDirective")).toString().trimmed();
    decision.promptDirective = state.value(QStringLiteral("promptDirective")).toString().trimmed();
    decision.reasonCode = state.value(QStringLiteral("reasonCode")).toString().trimmed();
    decision.strength = state.value(QStringLiteral("strength")).toDouble();
    decision.plannerMetadata = state;
    return decision;
}
