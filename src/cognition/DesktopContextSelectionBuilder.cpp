#include "cognition/DesktopContextSelectionBuilder.h"

#include <QRegularExpression>

namespace {
bool containsAny(const QString &input, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (input.contains(needle)) {
            return true;
        }
    }
    return false;
}

QString cleanedField(const QVariantMap &context, const QString &key)
{
    return context.value(key).toString().simplified();
}

QString hintField(QString value, int maxLength = 80)
{
    value = value.simplified();
    if (value.size() <= maxLength) {
        return value;
    }
    return value.left(maxLength).trimmed() + QStringLiteral("...");
}
}

QString DesktopContextSelectionBuilder::buildSelectionInput(const QString &userInput,
                                                            IntentType intent,
                                                            const QString &desktopSummary,
                                                            const QVariantMap &desktopContext,
                                                            qint64 contextAtMs,
                                                            qint64 nowMs,
                                                            bool privateModeEnabled)
{
    if (privateModeEnabled || desktopSummary.simplified().isEmpty()) {
        return userInput;
    }

    if (contextAtMs <= 0 || (nowMs - contextAtMs) > 90000) {
        return userInput;
    }

    if (!shouldUseDesktopContext(userInput, intent, desktopContext)) {
        return userInput;
    }

    const QString hint = buildHint(desktopSummary, desktopContext);
    if (hint.isEmpty()) {
        return userInput;
    }

    return QStringLiteral("%1\n\nCurrent desktop context: %2")
        .arg(userInput.trimmed(), hint);
}

QString DesktopContextSelectionBuilder::buildHint(const QString &desktopSummary,
                                                  const QVariantMap &desktopContext)
{
    QStringList parts;
    parts << desktopSummary.simplified();

    const QString taskId = cleanedField(desktopContext, QStringLiteral("taskId"));
    const QString topic = cleanedField(desktopContext, QStringLiteral("topic"));
    const QString appId = cleanedField(desktopContext, QStringLiteral("appId"));
    const QString threadId = cleanedField(desktopContext, QStringLiteral("threadId"));
    const QString document = cleanedField(desktopContext, QStringLiteral("documentContext"));
    const QString site = cleanedField(desktopContext, QStringLiteral("siteContext"));
    const QString workspace = cleanedField(desktopContext, QStringLiteral("workspaceContext"));
    const QString language = cleanedField(desktopContext, QStringLiteral("languageHint"));
    const QString metadataClass = cleanedField(desktopContext, QStringLiteral("metadataClass"));
    const QString workMode = inferredWorkMode(desktopContext);

    if (!taskId.isEmpty()) {
        parts << QStringLiteral("task=%1").arg(taskId);
    }
    if (!workMode.isEmpty()) {
        parts << QStringLiteral("mode=%1").arg(workMode);
    }
    if (!document.isEmpty() && document != QStringLiteral("private_mode_redacted")) {
        parts << QStringLiteral("document=%1").arg(hintField(document));
    }
    if (!site.isEmpty()) {
        parts << QStringLiteral("site=%1").arg(hintField(site));
    }
    if (!workspace.isEmpty()) {
        parts << QStringLiteral("workspace=%1").arg(hintField(workspace));
    }
    if (!language.isEmpty()) {
        parts << QStringLiteral("language=%1").arg(language);
    }
    if (!metadataClass.isEmpty()) {
        parts << QStringLiteral("class=%1").arg(metadataClass);
    }
    if (!topic.isEmpty()) {
        parts << QStringLiteral("topic=%1").arg(topic);
    }
    if (!appId.isEmpty()) {
        parts << QStringLiteral("app=%1").arg(appId);
    }
    if (!threadId.isEmpty()) {
        parts << QStringLiteral("thread=%1").arg(threadId);
    }

    return parts.join(QStringLiteral("; "));
}

QString DesktopContextSelectionBuilder::inferredWorkMode(const QVariantMap &desktopContext)
{
    const QString taskId = cleanedField(desktopContext, QStringLiteral("taskId")).toLower();
    const QString metadataClass = cleanedField(desktopContext, QStringLiteral("metadataClass")).toLower();
    const QString language = cleanedField(desktopContext, QStringLiteral("languageHint")).toLower();
    const QString site = cleanedField(desktopContext, QStringLiteral("siteContext")).toLower();

    if (metadataClass == QStringLiteral("private_app_only")) {
        return QStringLiteral("private");
    }
    if (taskId == QStringLiteral("editor_document")) {
        return language.isEmpty() ? QStringLiteral("document_editing") : QStringLiteral("coding");
    }
    if (taskId == QStringLiteral("browser_tab")) {
        if (containsAny(site, {
                QStringLiteral("github"),
                QStringLiteral("stackoverflow"),
                QStringLiteral("docs"),
                QStringLiteral("doc.")
            })) {
            return QStringLiteral("technical_research");
        }
        return QStringLiteral("web_research");
    }
    if (taskId == QStringLiteral("clipboard")) {
        return QStringLiteral("clipboard_review");
    }
    if (taskId == QStringLiteral("notification")) {
        return QStringLiteral("notification_triage");
    }
    return {};
}

bool DesktopContextSelectionBuilder::isNoisyClipboardContext(const QVariantMap &desktopContext)
{
    if (cleanedField(desktopContext, QStringLiteral("taskId")).toLower() != QStringLiteral("clipboard")) {
        return false;
    }

    const QString preview = cleanedField(desktopContext, QStringLiteral("clipboardPreview"));
    if (preview.isEmpty() || preview == QStringLiteral("private_mode_redacted")) {
        return true;
    }
    if (preview.startsWith(QStringLiteral("non_text:"), Qt::CaseInsensitive)) {
        return true;
    }
    return preview.size() < 6 && !preview.contains(QRegularExpression(QStringLiteral("[A-Za-z0-9]{3}")));
}

bool DesktopContextSelectionBuilder::shouldUseDesktopContext(const QString &userInput,
                                                             IntentType intent,
                                                             const QVariantMap &desktopContext)
{
    if (isNoisyClipboardContext(desktopContext)) {
        return false;
    }

    if (intent != IntentType::GENERAL_CHAT) {
        return true;
    }

    const QString lowered = userInput.trimmed().toLower();
    const QString taskId = cleanedField(desktopContext, QStringLiteral("taskId")).toLower();
    const bool refersToCurrentContext = containsAny(lowered, {
        QStringLiteral("this"),
        QStringLiteral("current"),
        QStringLiteral("here"),
        QStringLiteral("tab"),
        QStringLiteral("page"),
        QStringLiteral("window"),
        QStringLiteral("file"),
        QStringLiteral("document"),
        QStringLiteral("clipboard"),
        QStringLiteral("copied"),
        QStringLiteral("that")
    });

    if (refersToCurrentContext) {
        return true;
    }

    if (taskId == QStringLiteral("editor_document") || taskId == QStringLiteral("browser_tab")) {
        return containsAny(lowered, {
            QStringLiteral("summarize"),
            QStringLiteral("explain"),
            QStringLiteral("review"),
            QStringLiteral("fix"),
            QStringLiteral("edit"),
            QStringLiteral("read"),
            QStringLiteral("write"),
            QStringLiteral("project"),
            QStringLiteral("code"),
            QStringLiteral("repo"),
            QStringLiteral("log"),
            QStringLiteral("plan")
        });
    }

    if (taskId == QStringLiteral("clipboard")) {
        return containsAny(lowered, {
            QStringLiteral("paste"),
            QStringLiteral("copied"),
            QStringLiteral("clipboard")
        });
    }

    return false;
}
