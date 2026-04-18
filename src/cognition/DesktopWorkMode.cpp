#include "cognition/DesktopWorkMode.h"

namespace {
QString contextString(const QVariantMap &desktopContext, const QString &key)
{
    return desktopContext.value(key).toString().trimmed().toLower();
}

bool isTechnicalSite(const QString &siteContext)
{
    return siteContext.contains(QStringLiteral("github"))
        || siteContext.contains(QStringLiteral("stackoverflow"))
        || siteContext.contains(QStringLiteral("docs"))
        || siteContext.contains(QStringLiteral("doc."));
}
}

QString DesktopWorkMode::inferFromContext(const QVariantMap &desktopContext)
{
    const QString taskId = contextString(desktopContext, QStringLiteral("taskId"));
    const QString metadataClass = contextString(desktopContext, QStringLiteral("metadataClass"));
    const QString siteContext = contextString(desktopContext, QStringLiteral("siteContext"));
    const QString languageHint = contextString(desktopContext, QStringLiteral("languageHint"));

    if (metadataClass == QStringLiteral("private_app_only")) {
        return QStringLiteral("private");
    }
    if (taskId == QStringLiteral("editor_document")) {
        return languageHint.isEmpty() ? QStringLiteral("document_editing") : QStringLiteral("coding");
    }
    if (taskId == QStringLiteral("browser_tab")) {
        return isTechnicalSite(siteContext) ? QStringLiteral("technical_research") : QStringLiteral("web_research");
    }
    if (taskId == QStringLiteral("clipboard")) {
        return QStringLiteral("clipboard_review");
    }
    if (taskId == QStringLiteral("notification")) {
        return QStringLiteral("notification_triage");
    }
    return {};
}
