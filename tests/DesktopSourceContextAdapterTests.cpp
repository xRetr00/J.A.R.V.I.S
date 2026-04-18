#include <QtTest>

#include "perception/DesktopSourceContextAdapter.h"

class DesktopSourceContextAdapterTests : public QObject
{
    Q_OBJECT

private slots:
    void fillsBrowserTitleFallback();
    void preservesBrowserUiaFieldsAndAddsMissingSite();
    void fillsEditorWorkspaceFallback();
    void ignoresUnknownApps();
};

void DesktopSourceContextAdapterTests::fillsBrowserTitleFallback()
{
    const QVariantMap metadata = DesktopSourceContextAdapter::augmentMetadata(
        QStringLiteral("C:/Program Files/Google/Chrome/Application/chrome.exe"),
        QStringLiteral("Qt signals and slots | Qt Docs - Google Chrome"));

    QCOMPARE(metadata.value(QStringLiteral("documentContext")).toString(),
             QStringLiteral("Qt signals and slots"));
    QCOMPARE(metadata.value(QStringLiteral("siteContext")).toString(), QStringLiteral("Qt Docs"));
    QCOMPARE(metadata.value(QStringLiteral("sourceAdapterId")).toString(),
             QStringLiteral("browser_title_adapter.chrome.v1"));
    QVERIFY(metadata.value(QStringLiteral("metadataConfidence")).toDouble() >= 0.78);
}

void DesktopSourceContextAdapterTests::preservesBrowserUiaFieldsAndAddsMissingSite()
{
    QVariantMap existing;
    existing.insert(QStringLiteral("documentContext"), QStringLiteral("Selected Tab From UIA"));
    existing.insert(QStringLiteral("metadataSource"), QStringLiteral("uia_window_tree"));
    existing.insert(QStringLiteral("metadataConfidence"), 0.82);

    const QVariantMap metadata = DesktopSourceContextAdapter::augmentMetadata(
        QStringLiteral("C:/Program Files/Microsoft/Edge/Application/msedge.exe"),
        QStringLiteral("Fallback Page | Learn - Microsoft Edge"),
        existing);

    QCOMPARE(metadata.value(QStringLiteral("documentContext")).toString(),
             QStringLiteral("Selected Tab From UIA"));
    QCOMPARE(metadata.value(QStringLiteral("siteContext")).toString(), QStringLiteral("Learn"));
    QCOMPARE(metadata.value(QStringLiteral("metadataSource")).toString(),
             QStringLiteral("uia_window_tree+source_adapter"));
}

void DesktopSourceContextAdapterTests::fillsEditorWorkspaceFallback()
{
    const QVariantMap metadata = DesktopSourceContextAdapter::augmentMetadata(
        QStringLiteral("C:/Users/xRetro/AppData/Local/Programs/Microsoft VS Code/Code.exe"),
        QStringLiteral("DesktopPerceptionMonitor.cpp - Vaxil - Visual Studio Code"));

    QCOMPARE(metadata.value(QStringLiteral("documentContext")).toString(),
             QStringLiteral("DesktopPerceptionMonitor.cpp"));
    QCOMPARE(metadata.value(QStringLiteral("workspaceContext")).toString(), QStringLiteral("Vaxil"));
    QCOMPARE(metadata.value(QStringLiteral("languageHint")).toString(), QStringLiteral("cpp"));
    QCOMPARE(metadata.value(QStringLiteral("sourceAdapterId")).toString(),
             QStringLiteral("editor_title_adapter.vscode.v1"));
}

void DesktopSourceContextAdapterTests::ignoresUnknownApps()
{
    const QVariantMap metadata = DesktopSourceContextAdapter::augmentMetadata(
        QStringLiteral("unknown.exe"),
        QStringLiteral("Something - Unknown App"));

    QVERIFY(metadata.isEmpty());
}

QTEST_APPLESS_MAIN(DesktopSourceContextAdapterTests)
#include "DesktopSourceContextAdapterTests.moc"
