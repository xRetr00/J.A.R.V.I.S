#include "perception/DesktopSourceContextAdapter.h"

#include <algorithm>

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace {
QString normalizedAppFamily(const QString &appId)
{
    QString normalized = QFileInfo(appId.trimmed()).completeBaseName().trimmed().toLower();
    if (normalized == QStringLiteral("code")) {
        return QStringLiteral("vscode");
    }
    if (normalized == QStringLiteral("msedge")) {
        return QStringLiteral("edge");
    }
    return normalized;
}

bool isBrowserApp(const QString &app)
{
    static const QSet<QString> apps{
        QStringLiteral("chrome"), QStringLiteral("edge"), QStringLiteral("firefox"),
        QStringLiteral("brave"), QStringLiteral("opera")
    };
    return apps.contains(app);
}

bool isEditorApp(const QString &app)
{
    static const QSet<QString> apps{
        QStringLiteral("vscode"), QStringLiteral("cursor"), QStringLiteral("windsurf"),
        QStringLiteral("pycharm"), QStringLiteral("idea"), QStringLiteral("sublime_text")
    };
    return apps.contains(app);
}

QString cleanChunk(QString value)
{
    value = value.simplified();
    value.remove(QRegularExpression(QStringLiteral("\\s*\\([0-9]+\\)\\s*$")));
    value.remove(QRegularExpression(QStringLiteral("\\s*\\[[^\\]]{1,32}\\]\\s*$")));
    value.remove(QRegularExpression(QStringLiteral("^[\\-|:|•|»/\\s]+")));
    value.remove(QRegularExpression(QStringLiteral("[\\-|:|•|»/\\s]+$")));
    return value.simplified().left(96);
}

bool browserShellLabel(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("google chrome")
        || normalized == QStringLiteral("microsoft edge")
        || normalized == QStringLiteral("mozilla firefox")
        || normalized == QStringLiteral("brave")
        || normalized == QStringLiteral("opera")
        || normalized == QStringLiteral("chrome")
        || normalized == QStringLiteral("edge")
        || normalized == QStringLiteral("firefox");
}

QString languageForDocument(const QString &document)
{
    const QString extension = QFileInfo(document).suffix().toLower();
    if (extension == QStringLiteral("cpp") || extension == QStringLiteral("cc")) {
        return QStringLiteral("cpp");
    }
    if (extension == QStringLiteral("h") || extension == QStringLiteral("hpp")) {
        return QStringLiteral("cpp_header");
    }
    if (extension == QStringLiteral("py")) {
        return QStringLiteral("python");
    }
    if (extension == QStringLiteral("qml")) {
        return QStringLiteral("qml");
    }
    if (extension == QStringLiteral("ts") || extension == QStringLiteral("tsx")) {
        return QStringLiteral("typescript");
    }
    if (extension == QStringLiteral("md")) {
        return QStringLiteral("markdown");
    }
    if (extension == QStringLiteral("json")) {
        return QStringLiteral("json");
    }
    return {};
}

void fillIfMissing(QVariantMap &metadata, const QString &key, const QString &value, QStringList &filled)
{
    const QString cleaned = cleanChunk(value);
    if (cleaned.isEmpty() || !metadata.value(key).toString().trimmed().isEmpty()) {
        return;
    }
    metadata.insert(key, cleaned);
    filled << key;
}

void finalize(QVariantMap &metadata, const QString &adapterId, const QStringList &filled)
{
    if (filled.isEmpty()) {
        return;
    }
    const QString source = metadata.value(QStringLiteral("metadataSource")).toString();
    metadata.insert(QStringLiteral("metadataSource"),
                    source.isEmpty() ? QStringLiteral("source_adapter")
                                     : source + QStringLiteral("+source_adapter"));
    metadata.insert(QStringLiteral("sourceAdapterId"), adapterId);
    metadata.insert(QStringLiteral("sourceAdapterFilledFields"), filled);
    const double previous = metadata.value(QStringLiteral("metadataConfidence")).toDouble();
    metadata.insert(QStringLiteral("metadataConfidence"), std::clamp(std::max(previous, 0.74) + 0.04, 0.35, 0.9));
    metadata.insert(QStringLiteral("metadataQuality"),
                    metadata.value(QStringLiteral("metadataConfidence")).toDouble() >= 0.8
                        ? QStringLiteral("high")
                        : QStringLiteral("medium"));
}

QVariantMap augmentBrowser(const QString &app, const QString &title, QVariantMap metadata)
{
    QStringList filled;
    const QStringList shellParts = title.split(QRegularExpression(QStringLiteral("\\s+-\\s+")), Qt::SkipEmptyParts);
    const QString pageShell = shellParts.value(0);
    const QString maybeSite = shellParts.size() > 1 ? shellParts.at(shellParts.size() - 2) : QString();
    const QStringList pageParts = pageShell.split(QRegularExpression(QStringLiteral("\\s+\\|\\s+")), Qt::SkipEmptyParts);

    fillIfMissing(metadata, QStringLiteral("documentContext"), pageParts.value(0, pageShell), filled);
    if (pageParts.size() > 1) {
        fillIfMissing(metadata, QStringLiteral("siteContext"), pageParts.last(), filled);
    } else if (!browserShellLabel(maybeSite)) {
        fillIfMissing(metadata, QStringLiteral("siteContext"), maybeSite, filled);
    }
    metadata.insert(QStringLiteral("metadataClass"), QStringLiteral("browser_document"));
    metadata.insert(QStringLiteral("documentKind"), QStringLiteral("browser_page"));
    finalize(metadata, QStringLiteral("browser_title_adapter.%1.v1").arg(app), filled);
    return metadata;
}

QVariantMap augmentEditor(const QString &app, const QString &title, QVariantMap metadata)
{
    QStringList filled;
    const QStringList parts = title.split(QRegularExpression(QStringLiteral("\\s+-\\s+")), Qt::SkipEmptyParts);
    const QString document = parts.value(0);
    const QString workspace = parts.size() > 2 ? parts.value(1) : QString();

    fillIfMissing(metadata, QStringLiteral("documentContext"), document, filled);
    fillIfMissing(metadata, QStringLiteral("workspaceContext"), workspace, filled);
    fillIfMissing(metadata, QStringLiteral("languageHint"), languageForDocument(document), filled);
    metadata.insert(QStringLiteral("metadataClass"), QStringLiteral("editor_document"));
    metadata.insert(QStringLiteral("documentKind"), QStringLiteral("editor_file"));
    finalize(metadata, QStringLiteral("editor_title_adapter.%1.v1").arg(app), filled);
    return metadata;
}
}

QVariantMap DesktopSourceContextAdapter::augmentMetadata(const QString &appId,
                                                         const QString &windowTitle,
                                                         const QVariantMap &metadata)
{
    const QString app = normalizedAppFamily(appId);
    const QString title = windowTitle.simplified();
    if (title.isEmpty()) {
        return metadata;
    }
    if (isBrowserApp(app)) {
        return augmentBrowser(app, title, metadata);
    }
    if (isEditorApp(app)) {
        return augmentEditor(app, title, metadata);
    }
    return metadata;
}
