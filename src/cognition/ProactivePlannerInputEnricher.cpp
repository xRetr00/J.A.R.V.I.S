#include "cognition/ProactivePlannerInputEnricher.h"

#include <QDateTime>
#include <QMetaType>

namespace {
QString normalized(const QString &value)
{
    return value.simplified().left(120);
}

QString metadataString(const QVariantMap &metadata, const QString &key)
{
    return metadata.value(key).toString().trimmed();
}

QString firstValue(const QVariantMap &metadata, std::initializer_list<const char *> keys, QString *matchedKey = nullptr)
{
    for (const char *key : keys) {
        const QString name = QString::fromUtf8(key);
        const QString value = normalized(metadataString(metadata, name));
        if (!value.isEmpty()) {
            if (matchedKey != nullptr) {
                *matchedKey = name;
            }
            return value;
        }
    }
    return {};
}

QString sourceClass(const ProactivePlannerInputEnricher::Input &input)
{
    const QString source = input.sourceKind.trimmed().toLower();
    const QString connectorKind = metadataString(input.sourceMetadata, QStringLiteral("connectorKind"));
    if (!connectorKind.isEmpty() || source.contains(QStringLiteral("connector"))) {
        return QStringLiteral("connector_change");
    }
    if (source == QStringLiteral("desktop_context")) {
        return QStringLiteral("desktop_event");
    }
    if (source.contains(QStringLiteral("task")) || source.contains(QStringLiteral("reply"))) {
        return QStringLiteral("task_result");
    }
    return QStringLiteral("generic");
}

void insertIfMissing(QVariantMap &metadata, const QString &key, const QVariant &value)
{
    if (metadata.contains(key) || !value.isValid()) {
        return;
    }
    if (value.typeId() == QMetaType::QString && value.toString().trimmed().isEmpty()) {
        return;
    }
    metadata.insert(key, value);
}

QString fallbackLabelFromSummary(const QString &summary)
{
    const QString cleaned = normalized(summary);
    const int colon = cleaned.indexOf(QChar::fromLatin1(':'));
    if (colon >= 0 && colon + 1 < cleaned.size()) {
        return normalized(cleaned.mid(colon + 1));
    }
    return cleaned.left(96);
}

void enrichFreshness(QVariantMap &metadata, qint64 nowMs, QStringList &reasons)
{
    if (nowMs <= 0 || metadata.contains(QStringLiteral("eventAgeMs"))) {
        return;
    }
    const QString occurredAt = firstValue(metadata, {"occurredAtUtc", "finishedAt", "eventStartUtc"});
    const QDateTime timestamp = QDateTime::fromString(occurredAt, Qt::ISODateWithMs);
    if (!timestamp.isValid()) {
        return;
    }
    const qint64 ageMs = nowMs - timestamp.toMSecsSinceEpoch();
    if (ageMs < 0) {
        return;
    }
    metadata.insert(QStringLiteral("eventAgeMs"), ageMs);
    metadata.insert(QStringLiteral("eventFreshnessBand"),
                    ageMs <= 10 * 60 * 1000 ? QStringLiteral("fresh") : QStringLiteral("older"));
    reasons << QStringLiteral("freshness.event_age");
}

void mergeDesktopContext(QVariantMap &metadata, const QVariantMap &desktopContext, QStringList &reasons)
{
    if (desktopContext.isEmpty()) {
        return;
    }
    insertIfMissing(metadata, QStringLiteral("desktopTaskId"), desktopContext.value(QStringLiteral("taskId")));
    insertIfMissing(metadata, QStringLiteral("desktopThreadId"), desktopContext.value(QStringLiteral("threadId")));
    insertIfMissing(metadata, QStringLiteral("desktopTopic"), desktopContext.value(QStringLiteral("topic")));
    if (!metadata.contains(QStringLiteral("taskId"))) {
        insertIfMissing(metadata, QStringLiteral("taskId"), desktopContext.value(QStringLiteral("taskId")));
    }
    for (const QString &key : {
             QStringLiteral("documentContext"), QStringLiteral("siteContext"),
             QStringLiteral("workspaceContext"), QStringLiteral("languageHint")
         }) {
        insertIfMissing(metadata, key, desktopContext.value(key));
    }
    if (!desktopContext.value(QStringLiteral("taskId")).toString().trimmed().isEmpty()) {
        reasons << QStringLiteral("desktop.%1").arg(desktopContext.value(QStringLiteral("taskId")).toString().trimmed());
    }
}

bool containsAny(const QString &text, std::initializer_list<const char *> needles)
{
    for (const char *needle : needles) {
        if (text.contains(QString::fromUtf8(needle))) {
            return true;
        }
    }
    return false;
}

void enrichTaskResultPolicy(QVariantMap &metadata,
                            const ProactivePlannerInputEnricher::Input &input,
                            const QString &inputClass,
                            QStringList &reasons)
{
    if (inputClass != QStringLiteral("task_result")) {
        return;
    }

    const QString taskType = input.taskType.trimmed().toLower();
    const bool hasConnector = !metadataString(metadata, QStringLiteral("connectorKind")).isEmpty();
    const bool hasSummary = !input.resultSummary.trimmed().isEmpty();
    const bool hasSources = !input.sourceUrls.isEmpty();

    QString policy = QStringLiteral("generic_task_result");
    if (!input.success) {
        policy = QStringLiteral("failure_recovery");
    } else if (hasConnector) {
        policy = QStringLiteral("connector_task_result");
    } else if (hasSources || containsAny(taskType, {"web", "search", "research", "source"})) {
        policy = QStringLiteral("research_task_result");
    } else if (containsAny(taskType, {"file", "code", "dir", "document"})) {
        policy = QStringLiteral("document_task_result");
    } else if (!hasSummary || input.resultSummary.trimmed().size() < 8) {
        policy = QStringLiteral("low_signal_task_result");
        metadata.insert(QStringLiteral("sourceSpecificSuppressionReason"),
                        QStringLiteral("proposal.source_policy.low_signal_task_result"));
    }

    metadata.insert(QStringLiteral("sourceSpecificPolicy"), policy);
    reasons << QStringLiteral("source_policy.%1").arg(policy);
}
}

QVariantMap ProactivePlannerInputEnricher::enrich(const Input &input)
{
    QVariantMap metadata = input.sourceMetadata;
    QStringList reasons = metadata.value(QStringLiteral("plannerInputReasonCodes")).toStringList();
    const QString inputClass = sourceClass(input);
    metadata.insert(QStringLiteral("plannerInputClass"), inputClass);
    reasons << QStringLiteral("source_class.%1").arg(inputClass);

    if (inputClass == QStringLiteral("desktop_event")) {
        mergeDesktopContext(metadata, input.desktopContext, reasons);
    }
    if (!input.sourceUrls.isEmpty()) {
        metadata.insert(QStringLiteral("sourceUrlsPresent"), true);
        metadata.insert(QStringLiteral("sourceUrlCount"), input.sourceUrls.size());
        reasons << QStringLiteral("source_urls.present");
    }

    QString labelKey;
    QString label = firstValue(metadata, {
        "sourceLabel", "documentContext", "eventTitle", "subject", "sender",
        "bookmarkTitle", "title", "fileName", "siteContext", "taskKey"
    }, &labelKey);
    if (label.isEmpty()) {
        label = fallbackLabelFromSummary(input.resultSummary);
        labelKey = QStringLiteral("summary");
    }
    insertIfMissing(metadata, QStringLiteral("sourceLabel"), label);
    if (!label.isEmpty()) {
        reasons << QStringLiteral("label.%1").arg(labelKey);
    }

    QString keySource;
    QString keyHint = firstValue(metadata, {"presentationKeyHint", "taskKey", "eventId"}, &keySource);
    const QString connectorKind = metadataString(metadata, QStringLiteral("connectorKind")).toLower();
    if (keyHint.isEmpty() && !connectorKind.isEmpty() && !label.isEmpty()) {
        keyHint = QStringLiteral("%1:%2").arg(connectorKind, label.toLower());
        keySource = QStringLiteral("connector_label");
    }
    if (keyHint.isEmpty()) {
        keyHint = firstValue(metadata, {"desktopThreadId", "threadId"}, &keySource);
    }
    insertIfMissing(metadata, QStringLiteral("presentationKeyHint"), keyHint);
    if (!keyHint.isEmpty()) {
        reasons << QStringLiteral("key.%1").arg(keySource);
    }

    enrichTaskResultPolicy(metadata, input, inputClass, reasons);
    enrichFreshness(metadata, input.nowMs, reasons);
    reasons << (input.success ? QStringLiteral("result.success") : QStringLiteral("result.failure"));
    metadata.insert(QStringLiteral("plannerInputReasonCodes"), reasons);
    return metadata;
}
