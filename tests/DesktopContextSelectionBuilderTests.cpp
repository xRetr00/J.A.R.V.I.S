#include <QtTest>

#include "cognition/DesktopContextSelectionBuilder.h"

class DesktopContextSelectionBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void includesFreshEditorContextForWorkQueries();
    void skipsIrrelevantGeneralChat();
    void skipsPrivateModeContext();
};

void DesktopContextSelectionBuilderTests::includesFreshEditorContextForWorkQueries()
{
    QVariantMap context;
    context.insert(QStringLiteral("taskId"), QStringLiteral("editor_document"));
    context.insert(QStringLiteral("topic"), QStringLiteral("plan md"));
    context.insert(QStringLiteral("appId"), QStringLiteral("Code.exe"));
    context.insert(QStringLiteral("threadId"), QStringLiteral("editor_document::plan-md"));

    const QString selectionInput = DesktopContextSelectionBuilder::buildSelectionInput(
        QStringLiteral("summarize this"),
        IntentType::GENERAL_CHAT,
        QStringLiteral("Editor document: PLAN.md"),
        context,
        1000,
        1500,
        false);

    QVERIFY(selectionInput.contains(QStringLiteral("Current desktop context:")));
    QVERIFY(selectionInput.contains(QStringLiteral("PLAN.md")));
    QVERIFY(selectionInput.contains(QStringLiteral("task=editor_document")));
}

void DesktopContextSelectionBuilderTests::skipsIrrelevantGeneralChat()
{
    QVariantMap context;
    context.insert(QStringLiteral("taskId"), QStringLiteral("editor_document"));

    const QString selectionInput = DesktopContextSelectionBuilder::buildSelectionInput(
        QStringLiteral("tell me a joke"),
        IntentType::GENERAL_CHAT,
        QStringLiteral("Editor document: PLAN.md"),
        context,
        1000,
        1500,
        false);

    QCOMPARE(selectionInput, QStringLiteral("tell me a joke"));
}

void DesktopContextSelectionBuilderTests::skipsPrivateModeContext()
{
    QVariantMap context;
    context.insert(QStringLiteral("taskId"), QStringLiteral("browser_tab"));

    const QString selectionInput = DesktopContextSelectionBuilder::buildSelectionInput(
        QStringLiteral("summarize this"),
        IntentType::GENERAL_CHAT,
        QStringLiteral("Browser tab: release notes"),
        context,
        1000,
        1500,
        true);

    QCOMPARE(selectionInput, QStringLiteral("summarize this"));
}

QTEST_APPLESS_MAIN(DesktopContextSelectionBuilderTests)
#include "DesktopContextSelectionBuilderTests.moc"
