#include <QtTest>

#include "cognition/PromptContextTemporalPolicy.h"

class PromptContextTemporalPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void keepsFullContextWhenFresh();
    void trimsLowSignalBlocksWhenStable();
};

namespace {
MemoryRecord makeRecord(const QString &key, const QString &value, const QString &source)
{
    return MemoryRecord{
        .type = QStringLiteral("prompt_context"),
        .key = key,
        .value = value,
        .confidence = 0.9f,
        .source = source,
        .updatedAt = QStringLiteral("1")
    };
}
}

void PromptContextTemporalPolicyTests::keepsFullContextWhenFresh()
{
    const QList<MemoryRecord> records = {
        makeRecord(QStringLiteral("desktop_prompt_summary"),
                   QStringLiteral("Desktop context: editing CooldownEngine.cpp."),
                   QStringLiteral("prompt.desktop_summary")),
        makeRecord(QStringLiteral("desktop_prompt_task"),
                   QStringLiteral("Task type: editor_document."),
                   QStringLiteral("prompt.task_alignment")),
        makeRecord(QStringLiteral("desktop_prompt_document"),
                   QStringLiteral("Document: CooldownEngine.cpp."),
                   QStringLiteral("prompt.document_relevance")),
        makeRecord(QStringLiteral("desktop_prompt_workspace"),
                   QStringLiteral("Workspace: D:/Vaxil/src/cognition."),
                   QStringLiteral("prompt.workspace_relevance")),
        makeRecord(QStringLiteral("desktop_prompt_app"),
                   QStringLiteral("App: cursor."),
                   QStringLiteral("prompt.app_relevance"))
    };

    const PromptContextTemporalDecision decision = PromptContextTemporalPolicy::apply(records, 0, 0);
    QCOMPARE(decision.reasonCode, QStringLiteral("prompt_context.full_context"));
    QVERIFY(decision.suppressedKeys.isEmpty());
    QCOMPARE(decision.selectedRecords.size(), records.size());
    QVERIFY(decision.promptContext.contains(QStringLiteral("Desktop context: editing CooldownEngine.cpp.")));
    QVERIFY(decision.promptContext.contains(QStringLiteral("App: cursor.")));
}

void PromptContextTemporalPolicyTests::trimsLowSignalBlocksWhenStable()
{
    const QList<MemoryRecord> records = {
        makeRecord(QStringLiteral("desktop_prompt_summary"),
                   QStringLiteral("Desktop context: editing CooldownEngine.cpp."),
                   QStringLiteral("prompt.desktop_summary")),
        makeRecord(QStringLiteral("desktop_prompt_task"),
                   QStringLiteral("Task type: editor_document."),
                   QStringLiteral("prompt.task_alignment")),
        makeRecord(QStringLiteral("desktop_prompt_document"),
                   QStringLiteral("Document: CooldownEngine.cpp."),
                   QStringLiteral("prompt.document_relevance")),
        makeRecord(QStringLiteral("desktop_prompt_workspace"),
                   QStringLiteral("Workspace: D:/Vaxil/src/cognition."),
                   QStringLiteral("prompt.workspace_relevance")),
        makeRecord(QStringLiteral("desktop_prompt_app"),
                   QStringLiteral("App: cursor."),
                   QStringLiteral("prompt.app_relevance")),
        makeRecord(QStringLiteral("desktop_prompt_thread"),
                   QStringLiteral("Thread: editor_document:cooldown_engine."),
                   QStringLiteral("prompt.thread_continuity"))
    };

    const PromptContextTemporalDecision decision = PromptContextTemporalPolicy::apply(records, 2, 150000);
    QCOMPARE(decision.reasonCode, QStringLiteral("prompt_context.stable_context_trimmed"));
    QVERIFY(decision.suppressedKeys.contains(QStringLiteral("desktop_prompt_summary")));
    QVERIFY(decision.suppressedKeys.contains(QStringLiteral("desktop_prompt_app")));
    QVERIFY(decision.suppressedKeys.contains(QStringLiteral("desktop_prompt_thread")));
    QVERIFY(!decision.promptContext.contains(QStringLiteral("Desktop context: editing CooldownEngine.cpp.")));
    QVERIFY(!decision.promptContext.contains(QStringLiteral("App: cursor.")));
    QVERIFY(decision.promptContext.contains(QStringLiteral("Document: CooldownEngine.cpp.")));
    QVERIFY(decision.promptContext.contains(QStringLiteral("Workspace: D:/Vaxil/src/cognition.")));
}

QTEST_APPLESS_MAIN(PromptContextTemporalPolicyTests)
#include "PromptContextTemporalPolicyTests.moc"
