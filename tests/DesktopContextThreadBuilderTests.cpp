#include <QtTest>

#include "perception/DesktopContextThreadBuilder.h"

class DesktopContextThreadBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsActiveWindowContext();
    void buildsClipboardContext();
    void buildsNotificationContext();
};

void DesktopContextThreadBuilderTests::buildsActiveWindowContext()
{
    const CompanionContextSnapshot snapshot = DesktopContextThreadBuilder::fromActiveWindow(
        QStringLiteral("C:/Program Files/Microsoft VS Code/Code.exe"),
        QStringLiteral("PLAN.md - Visual Studio Code"));

    QCOMPARE(snapshot.appId, QStringLiteral("vscode"));
    QCOMPARE(snapshot.taskId, QStringLiteral("editor_document"));
    QVERIFY(snapshot.threadId.value.startsWith(QStringLiteral("desktop::editor_document::vscode::")));
    QCOMPARE(snapshot.topic, QStringLiteral("plan_md"));
    QCOMPARE(snapshot.recentIntent, QStringLiteral("reference current file"));
    QVERIFY(DesktopContextThreadBuilder::describeContext(snapshot).contains(QStringLiteral("editor file")));
}

void DesktopContextThreadBuilderTests::buildsClipboardContext()
{
    const CompanionContextSnapshot snapshot = DesktopContextThreadBuilder::fromClipboard(
        QStringLiteral("chrome.exe"),
        QStringLiteral("ChatGPT | OpenAI"),
        QStringLiteral("OpenAI docs and structured output examples"));

    QCOMPARE(snapshot.appId, QStringLiteral("chrome"));
    QCOMPARE(snapshot.taskId, QStringLiteral("clipboard"));
    QVERIFY(snapshot.threadId.value.startsWith(QStringLiteral("desktop::clipboard::chrome::")));
    QCOMPARE(snapshot.topic, QStringLiteral("openai_docs_and_structured_output_exampl"));
}

void DesktopContextThreadBuilderTests::buildsNotificationContext()
{
    const CompanionContextSnapshot snapshot = DesktopContextThreadBuilder::fromNotification(
        QStringLiteral("Startup blocked"),
        QStringLiteral("Whisper executable missing"),
        QStringLiteral("high"));

    QCOMPARE(snapshot.appId, QStringLiteral("vaxil"));
    QCOMPARE(snapshot.taskId, QStringLiteral("notification"));
    QCOMPARE(snapshot.topic, QStringLiteral("startup_blocked"));
    QVERIFY(snapshot.confidence > 0.8);
}

QTEST_APPLESS_MAIN(DesktopContextThreadBuilderTests)

#include "DesktopContextThreadBuilderTests.moc"
