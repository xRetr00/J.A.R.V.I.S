#include <QtTest>

#include "ai/PromptAdapter.h"

namespace {
AssistantIdentity identity()
{
    return AssistantIdentity{
        .assistantName = QStringLiteral("Vaxil"),
        .personality = QStringLiteral("desktop companion"),
        .tone = QStringLiteral("direct"),
        .addressingStyle = QStringLiteral("natural")
    };
}

UserProfile userProfile()
{
    return UserProfile{.userName = QStringLiteral("Tester")};
}

AgentToolSpec tool(const QString &name)
{
    return AgentToolSpec{
        .name = name,
        .description = QStringLiteral("Test tool %1").arg(name),
        .parameters = nlohmann::json::object()
    };
}

QString systemPrompt(const QList<AiMessage> &messages)
{
    return messages.isEmpty() ? QString{} : messages.first().content;
}
}

class PromptAssemblyTests : public QObject
{
    Q_OBJECT

private slots:
    void normalChatPromptIsMinimal();
    void agentPromptIncludesOnlySelectedTools();
    void continuationPromptPreservesEvidence();
    void promptReportIncludesReasonsAndCounts();
};

void PromptAssemblyTests::normalChatPromptIsMinimal()
{
    PromptAdapter adapter;
    PromptTurnContext context;
    context.userInput = QStringLiteral("How are you?");
    context.identity = identity();
    context.userProfile = userProfile();
    context.compactResponseContract = QStringLiteral("Answer briefly.");

    const QString prompt = systemPrompt(adapter.buildConversationMessages(context));

    QVERIFY(prompt.contains(QStringLiteral("<identity>")));
    QVERIFY(prompt.contains(QStringLiteral("<behavior_mode>")));
    QVERIFY(prompt.contains(QStringLiteral("<memory_context>")));
    QVERIFY(!prompt.contains(QStringLiteral("<tools>")));
    QVERIFY(!prompt.contains(QStringLiteral("<workspace>")));
    QVERIFY(!prompt.contains(QStringLiteral("<logs>")));
    QVERIFY(!prompt.contains(QStringLiteral("<examples>")));
    QVERIFY(prompt.size() < 2600);
}

void PromptAssemblyTests::agentPromptIncludesOnlySelectedTools()
{
    PromptAdapter adapter;
    PromptTurnContext context;
    context.userInput = QStringLiteral("Search the web for current Qt releases");
    context.identity = identity();
    context.userProfile = userProfile();
    context.allowedTools = {tool(QStringLiteral("web_search"))};
    context.compactResponseContract = QStringLiteral("Return adapter JSON.");

    const QString prompt = systemPrompt(adapter.buildHybridAgentMessages(context));

    QVERIFY(prompt.contains(QStringLiteral("<tools>")));
    QVERIFY(prompt.contains(QStringLiteral("web_search")));
    QVERIFY(!prompt.contains(QStringLiteral("file_read")));
    QVERIFY(!prompt.contains(QStringLiteral("<workspace>")));
    QVERIFY(!prompt.contains(QStringLiteral("<examples>")));
    QVERIFY(prompt.size() < 5200);
}

void PromptAssemblyTests::continuationPromptPreservesEvidence()
{
    PromptAdapter adapter;
    PromptTurnContext context;
    context.userInput = QStringLiteral("What did you see?");
    context.identity = identity();
    context.userProfile = userProfile();
    context.allowedTools = {tool(QStringLiteral("browser_fetch_text"))};
    context.activeTaskState = QStringLiteral("active_thread=thread-1\ncontinuation=true");
    context.verifiedEvidence = QStringLiteral("Course A - Machine Learning Basics");
    context.compactResponseContract = QStringLiteral("Continue from evidence.");
    context.toolResults = {
        AgentToolResult{
            .callId = QStringLiteral("call-1"),
            .toolName = QStringLiteral("browser_fetch_text"),
            .output = QStringLiteral("Course A - Machine Learning Basics"),
            .success = true,
            .errorKind = ToolErrorKind::None
        }
    };

    const QString prompt = systemPrompt(adapter.buildHybridAgentContinuationMessages(context));

    QVERIFY(prompt.contains(QStringLiteral("<task_state>")));
    QVERIFY(prompt.contains(QStringLiteral("<verified_evidence>")));
    QVERIFY(prompt.contains(QStringLiteral("Course A")));
    QVERIFY(prompt.contains(QStringLiteral("<adapter_loop>")));
}

void PromptAssemblyTests::promptReportIncludesReasonsAndCounts()
{
    PromptAdapter adapter;
    PromptTurnContext context;
    context.userInput = QStringLiteral("Read the logs");
    context.identity = identity();
    context.userProfile = userProfile();
    context.allowedTools = {tool(QStringLiteral("log_tail"))};
    context.includeLogContext = true;
    context.workspaceRoot = QStringLiteral("D:/Vaxil");
    context.verifiedEvidence = QStringLiteral("No fatal errors.");
    context.compactResponseContract = QStringLiteral("Summarize grounded facts.");

    const PromptAssemblyReport report = adapter.buildPromptAssemblyReport(context);

    QVERIFY(report.totalPromptChars > 0);
    QVERIFY(report.selectedToolNames.contains(QStringLiteral("log_tail")));
    QCOMPARE(report.evidenceCount, 1);
    QVERIFY(!report.includedBlocks.isEmpty());
    QVERIFY(report.toLogString().contains(QStringLiteral("prompt_assembly")));
    QVERIFY(report.toLogString().contains(QStringLiteral("prompt.logs_relevant")));
}

QTEST_APPLESS_MAIN(PromptAssemblyTests)
#include "PromptAssemblyTests.moc"
