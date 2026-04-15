#include "memory/MemoryStore.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

#include "cognition/CompiledContextHistoryPolicy.h"

namespace {
int compiledContextPolicyScore(const QString &query, const QVariantMap &state)
{
    const QString haystack = QStringList{
        state.value(QStringLiteral("dominantMode")).toString(),
        state.value(QStringLiteral("selectionDirective")).toString(),
        state.value(QStringLiteral("promptDirective")).toString(),
        state.value(QStringLiteral("reasonCode")).toString()
    }.join(QLatin1Char(' ')).toLower();

    int score = 0;
    if (query.isEmpty()) {
        score += 20;
    } else if (haystack.contains(query)) {
        score += 120;
    }
    score += static_cast<int>(state.value(QStringLiteral("strength")).toDouble() * 10.0);
    return score;
}
}

QList<MemoryRecord> MemoryStore::compiledContextPolicyMemory(const QString &query) const
{
    const QVariantMap state = compiledContextPolicyState();
    if (state.isEmpty()) {
        return {};
    }

    QString normalized = query.trimmed().toLower();
    normalized.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral(" "));
    normalized = normalized.simplified();
    if (compiledContextPolicyScore(normalized, state) <= 0) {
        return {};
    }

    MemoryRecord record = CompiledContextHistoryPolicy::buildContextRecord(
        CompiledContextHistoryPolicy::fromState(state));
    if (record.key.trimmed().isEmpty()) {
        return {};
    }
    record.source = QStringLiteral("compiled_history_policy_memory");
    record.updatedAt = QString::number(state.value(QStringLiteral("updatedAtMs")).toLongLong());
    return {record};
}

bool MemoryStore::upsertCompiledContextPolicyState(const QVariantMap &state)
{
    if (state.isEmpty()) {
        return false;
    }

    const QJsonDocument document(QJsonObject::fromVariantMap(state));
    MemoryEntry entry;
    entry.type = MemoryType::Context;
    entry.kind = QStringLiteral("context");
    entry.key = compiledContextPolicyStorageKey();
    entry.title = QStringLiteral("compiled_context_policy");
    entry.value = QString::fromUtf8(document.toJson(QJsonDocument::Compact));
    entry.content = entry.value;
    entry.id = QStringLiteral("compiled-context-policy");
    entry.confidence = 0.94f;
    entry.source = QStringLiteral("compiled_history_policy");
    entry.tags = {
        QStringLiteral("compiled_context_policy"),
        state.value(QStringLiteral("dominantMode")).toString().trimmed()
    };
    entry.createdAt = QDateTime::currentDateTimeUtc();
    entry.updatedAt = entry.createdAt.toUTC().toString(Qt::ISODate);
    return upsertEntry(entry);
}

bool MemoryStore::deleteCompiledContextPolicyState()
{
    return deleteEntry(compiledContextPolicyStorageKey());
}

QVariantMap MemoryStore::compiledContextPolicyState() const
{
    for (const MemoryEntry &entry : allEntries()) {
        if (entry.source != QStringLiteral("compiled_history_policy")) {
            continue;
        }
        if (entry.key != compiledContextPolicyStorageKey()) {
            continue;
        }

        const QJsonDocument document = QJsonDocument::fromJson(entry.value.toUtf8());
        if (!document.isObject()) {
            return {};
        }
        return document.object().toVariantMap();
    }
    return {};
}

QString MemoryStore::compiledContextPolicyStorageKey() const
{
    return QStringLiteral("compiled_context_policy_state");
}
