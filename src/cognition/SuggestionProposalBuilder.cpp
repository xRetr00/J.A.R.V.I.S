#include "cognition/SuggestionProposalBuilder.h"

namespace {
QString normalizedTaskType(const QString &taskType)
{
    return taskType.trimmed().toLower();
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

QString metadataString(const SuggestionProposalBuilder::Input &input, const QString &key)
{
    return input.sourceMetadata.value(key).toString().trimmed();
}

QString metadataLower(const SuggestionProposalBuilder::Input &input, const QString &key)
{
    return metadataString(input, key).toLower();
}

QString firstMetadataString(const SuggestionProposalBuilder::Input &input,
                            std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value = metadataString(input, QString::fromUtf8(key)).simplified();
        if (!value.isEmpty()) {
            return value.left(96);
        }
    }
    return {};
}

QString effectiveDesktopTask(const SuggestionProposalBuilder::Input &input, const QString &taskType)
{
    const QString metadataTask = metadataLower(input, QStringLiteral("taskId"));
    return metadataTask.isEmpty() ? taskType : metadataTask;
}

QString effectiveConnectorKind(const SuggestionProposalBuilder::Input &input, const QString &taskType)
{
    const QString connectorKind = metadataLower(input, QStringLiteral("connectorKind"));
    if (!connectorKind.isEmpty()) {
        return connectorKind;
    }
    if (containsAny(taskType, {"calendar", "schedule", "meeting", "timer", "reminder"})) {
        return QStringLiteral("schedule");
    }
    if (containsAny(taskType, {"email", "mail", "message", "inbox"})) {
        return QStringLiteral("inbox");
    }
    if (containsAny(taskType, {"note", "memo", "draft", "write"})) {
        return QStringLiteral("notes");
    }
    if (taskType.contains(QStringLiteral("file")) || taskType.contains(QStringLiteral("code"))) {
        return {};
    }
    if (containsAny(taskType, {"research", "search", "web", "browser", "source"})) {
        return QStringLiteral("research");
    }
    return {};
}

QString presentationKeyHint(const SuggestionProposalBuilder::Input &input)
{
    const QString explicitKey = firstMetadataString(input, {"presentationKeyHint", "presentationKey", "taskKey", "eventId"});
    if (!explicitKey.isEmpty()) {
        return explicitKey;
    }
    const QString connectorKind = metadataLower(input, QStringLiteral("connectorKind"));
    const QString item = firstMetadataString(input, {"fileName", "subject", "eventTitle", "bookmarkTitle", "title"});
    if (!connectorKind.isEmpty() && !item.isEmpty()) {
        return QStringLiteral("%1:%2").arg(connectorKind, item.toLower());
    }
    const QString desktopTask = metadataLower(input, QStringLiteral("taskId"));
    const QString document = firstMetadataString(input, {"documentContext", "siteContext", "workspaceContext"});
    if (!desktopTask.isEmpty() && !document.isEmpty()) {
        return QStringLiteral("%1:%2").arg(desktopTask, document.toLower());
    }
    return {};
}

QString reasonCodeFor(const QString &capabilityId, const SuggestionProposalBuilder::Input &input)
{
    const QString task = metadataLower(input, QStringLiteral("taskId"));
    const QString connector = metadataLower(input, QStringLiteral("connectorKind"));
    if (!connector.isEmpty()) {
        return QStringLiteral("proposal.metadata.%1").arg(connector);
    }
    if (task == QStringLiteral("editor_document") || task == QStringLiteral("browser_tab")) {
        return QStringLiteral("proposal.metadata.%1").arg(task);
    }
    return QStringLiteral("proposal.heuristic.%1").arg(capabilityId);
}

QString contextualTitle(const QString &capabilityId, const QString &fallback, const SuggestionProposalBuilder::Input &input)
{
    if (capabilityId == QStringLiteral("document_follow_up")) {
        const QString label = firstMetadataString(input, {"documentContext", "fileName", "title"});
        return label.isEmpty() ? fallback : QStringLiteral("Help with %1").arg(label);
    }
    if (capabilityId == QStringLiteral("browser_follow_up")) {
        const QString label = firstMetadataString(input, {"siteContext", "documentContext", "bookmarkTitle", "title"});
        return label.isEmpty() ? fallback : QStringLiteral("Use %1").arg(label);
    }
    if (capabilityId == QStringLiteral("schedule_follow_up")) {
        const QString label = firstMetadataString(input, {"eventTitle", "title"});
        return label.isEmpty() ? fallback : QStringLiteral("Review %1").arg(label);
    }
    if (capabilityId == QStringLiteral("inbox_follow_up")) {
        const QString label = firstMetadataString(input, {"subject", "sender", "title"});
        return label.isEmpty() ? fallback : QStringLiteral("Triage %1").arg(label);
    }
    if (capabilityId == QStringLiteral("note_follow_up")) {
        const QString label = firstMetadataString(input, {"title", "fileName"});
        return label.isEmpty() ? fallback : QStringLiteral("Capture %1").arg(label);
    }
    if (capabilityId == QStringLiteral("source_review")) {
        const QString label = firstMetadataString(input, {"bookmarkTitle", "query", "title", "siteContext"});
        return label.isEmpty() ? fallback : QStringLiteral("Review %1").arg(label);
    }
    return fallback;
}

QString contextualSummary(const QString &capabilityId, const QString &fallback, const SuggestionProposalBuilder::Input &input)
{
    const QString workspace = firstMetadataString(input, {"workspaceContext"});
    if (capabilityId == QStringLiteral("document_follow_up") && !workspace.isEmpty()) {
        return QStringLiteral("I can review this file in %1, explain it, or help plan the next edit.").arg(workspace);
    }
    const QString site = firstMetadataString(input, {"siteContext"});
    if (capabilityId == QStringLiteral("browser_follow_up") && !site.isEmpty()) {
        return QStringLiteral("I can summarize this %1 page, compare it with sources, or extract useful details.").arg(site);
    }
    const QString start = firstMetadataString(input, {"eventStartUtc"});
    if (capabilityId == QStringLiteral("schedule_follow_up") && !start.isEmpty()) {
        return QStringLiteral("I can turn this timed schedule update into a reminder, prep plan, or quick summary.");
    }
    const QString sender = firstMetadataString(input, {"sender"});
    if (capabilityId == QStringLiteral("inbox_follow_up") && !sender.isEmpty()) {
        return QStringLiteral("I can summarize this message from %1 or help decide if it needs a reply.").arg(sender);
    }
    return fallback;
}
}

QList<ActionProposal> SuggestionProposalBuilder::build(const Input &input)
{
    QList<ActionProposal> proposals;
    const QString taskType = normalizedTaskType(input.taskType);
    const QString desktopTask = effectiveDesktopTask(input, taskType);
    const QString connectorKind = effectiveConnectorKind(input, taskType);

    if (!input.sourceUrls.isEmpty()) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("source_review"),
                                  QStringLiteral("Review sources"),
                                  QStringLiteral("I can open one of the sources or summarize the findings."),
                                  QStringLiteral("medium"),
                                  input));
    }

    if (taskType == QStringLiteral("clipboard")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("clipboard_follow_up"),
                                  QStringLiteral("Use clipboard"),
                                  QStringLiteral("I can summarize what you copied or turn it into a quick note."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (taskType == QStringLiteral("notification")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("notification_triage"),
                                  QStringLiteral("Triage notification"),
                                  QStringLiteral("I can summarize this notification or help you decide what matters."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (taskType == QStringLiteral("web_search") || taskType == QStringLiteral("web_fetch")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("web_follow_up"),
                                  QStringLiteral("Compare findings"),
                                  QStringLiteral("I can compare the sources, extract the key points, or open one."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (containsAny(taskType, {"calendar", "schedule", "meeting", "timer", "reminder"})) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("schedule_follow_up"),
                                  QStringLiteral("Review schedule"),
                                  QStringLiteral("I can turn this into a reminder, a short plan, or a quick schedule summary."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (containsAny(taskType, {"email", "mail", "message", "inbox"})) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("inbox_follow_up"),
                                  QStringLiteral("Triage messages"),
                                  QStringLiteral("I can summarize the important messages or help you decide what needs a reply."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (containsAny(taskType, {"note", "memo", "draft", "write"})) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("note_follow_up"),
                                  QStringLiteral("Capture note"),
                                  QStringLiteral("I can turn this into a short note, checklist, or saved summary."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (taskType.contains(QStringLiteral("file")) || taskType.contains(QStringLiteral("code")) || taskType == QStringLiteral("dir_list")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("document_follow_up"),
                                  QStringLiteral("Inspect files"),
                                  QStringLiteral("I can pull out the important parts, compare files, or turn them into a short summary."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (taskType.contains(QStringLiteral("browser")) || taskType.contains(QStringLiteral("page")) || taskType.contains(QStringLiteral("computer_open"))) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("browser_follow_up"),
                                  QStringLiteral("Continue in browser"),
                                  QStringLiteral("I can open the relevant page again or extract the useful details."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (!taskType.isEmpty()) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("task_follow_up"),
                                  QStringLiteral("Continue task"),
                                  QStringLiteral("I can take the next step or turn this into a short summary."),
                                  QStringLiteral("low"),
                                  input));
    }

    if (desktopTask == QStringLiteral("editor_document")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("document_follow_up"),
                                  QStringLiteral("Help with current file"),
                                  QStringLiteral("I can review the current file, explain it, or help plan the next edit."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (desktopTask == QStringLiteral("browser_tab")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("browser_follow_up"),
                                  QStringLiteral("Use current page"),
                                  QStringLiteral("I can summarize this page, compare it with sources, or extract useful details."),
                                  QStringLiteral("medium"),
                                  input));
    }

    if (connectorKind == QStringLiteral("schedule")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("schedule_follow_up"),
                                  QStringLiteral("Review schedule"),
                                  QStringLiteral("I can turn this update into a reminder, short plan, or schedule summary."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (connectorKind == QStringLiteral("inbox")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("inbox_follow_up"),
                                  QStringLiteral("Triage messages"),
                                  QStringLiteral("I can summarize this inbox update or help decide what needs a reply."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (connectorKind == QStringLiteral("notes")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("note_follow_up"),
                                  QStringLiteral("Capture note"),
                                  QStringLiteral("I can turn this note update into a checklist or saved summary."),
                                  QStringLiteral("medium"),
                                  input));
    } else if (connectorKind == QStringLiteral("research")) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("source_review"),
                                  QStringLiteral("Review research"),
                                  QStringLiteral("I can summarize this research update or compare it with the current task."),
                                  QStringLiteral("medium"),
                                  input));
    }

    if (!input.success) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("failure_recovery"),
                                  QStringLiteral("Recover from failure"),
                                  QStringLiteral("I can try a different approach or troubleshoot what failed."),
                                  QStringLiteral("high"),
                                  input));
    } else if (!input.resultSummary.trimmed().isEmpty()) {
        appendUnique(proposals,
                     makeProposal(QStringLiteral("result_summary"),
                                  QStringLiteral("Summarize result"),
                                  QStringLiteral("I can turn this result into a short summary or checklist."),
                                  QStringLiteral("low"),
                                  input));
    }

    return proposals;
}

void SuggestionProposalBuilder::appendUnique(QList<ActionProposal> &proposals, const ActionProposal &proposal)
{
    for (const ActionProposal &existing : proposals) {
        if (existing.summary.compare(proposal.summary, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    proposals.push_back(proposal);
}

ActionProposal SuggestionProposalBuilder::makeProposal(const QString &capabilityId,
                                                       const QString &title,
                                                       const QString &summary,
                                                       const QString &priority,
                                                       const Input &input)
{
    ActionProposal proposal;
    const QString effectiveTitle = contextualTitle(capabilityId, title, input);
    const QString effectiveSummary = contextualSummary(capabilityId, summary, input);
    proposal.proposalId = QStringLiteral("%1:%2:%3")
                              .arg(capabilityId,
                                   input.taskType.trimmed().isEmpty() ? QStringLiteral("task") : input.taskType.trimmed(),
                                   input.sourceKind.trimmed().isEmpty() ? QStringLiteral("default") : input.sourceKind.trimmed());
    proposal.capabilityId = capabilityId;
    proposal.title = effectiveTitle;
    proposal.summary = effectiveSummary;
    proposal.priority = priority;
    proposal.arguments = {
        {QStringLiteral("sourceKind"), input.sourceKind},
        {QStringLiteral("taskType"), input.taskType},
        {QStringLiteral("success"), input.success},
        {QStringLiteral("sourceCount"), input.sourceUrls.size()},
        {QStringLiteral("proposalReasonCode"), reasonCodeFor(capabilityId, input)}
    };
    const QString keyHint = presentationKeyHint(input);
    if (!keyHint.isEmpty()) {
        proposal.arguments.insert(QStringLiteral("presentationKeyHint"), keyHint);
    }
    const QString sourceLabel = firstMetadataString(input, {
        "documentContext", "eventTitle", "subject", "bookmarkTitle", "title", "fileName", "siteContext"
    });
    if (!sourceLabel.isEmpty()) {
        proposal.arguments.insert(QStringLiteral("sourceLabel"), sourceLabel);
    }
    for (const QString &key : {
             QStringLiteral("taskId"), QStringLiteral("connectorKind"), QStringLiteral("producerId"),
             QStringLiteral("documentContext"), QStringLiteral("siteContext"), QStringLiteral("workspaceContext"),
             QStringLiteral("eventTitle"), QStringLiteral("eventStartUtc"), QStringLiteral("sender"),
             QStringLiteral("subject"), QStringLiteral("title"), QStringLiteral("fileName"), QStringLiteral("bookmarkTitle")
         }) {
        const QVariant value = input.sourceMetadata.value(key);
        if (value.isValid() && !value.toString().trimmed().isEmpty()) {
            proposal.arguments.insert(key, value);
        }
    }
    return proposal;
}
