#include "cognition/DesktopContextSelectionBuilder.h"

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

    if (!taskId.isEmpty()) {
        parts << QStringLiteral("task=%1").arg(taskId);
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

bool DesktopContextSelectionBuilder::shouldUseDesktopContext(const QString &userInput,
                                                             IntentType intent,
                                                             const QVariantMap &desktopContext)
{
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
