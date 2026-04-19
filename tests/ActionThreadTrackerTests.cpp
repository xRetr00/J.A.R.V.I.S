#include <QtTest>
#include <QTemporaryDir>

#include "core/ActionThreadTracker.h"

class ActionThreadTrackerTests : public QObject
{
    Q_OBJECT

private slots:
    void beginCreatesRunningThread();
    void rememberResultUpdatesExistingThreadAndMapsSuccess();
    void rememberResultMapsFailure();
    void rememberResultMapsCanceled();
    void rememberReplyCreatesCompletedAndFailedThread();
    void buildContinuationInputPreservesLegacyPromptAndFallback();
    void sanitizesRecursiveContinuationEnvelope();
    void buildCompletionInputPreservesLegacyPromptAndFallback();
    void clearRemovesCurrentThread();
    void isCurrentUsableReturnsFalseForMissingOrExpiredThread();
    void recentThreadsSurviveClearAndShortRestart();
};

void ActionThreadTrackerTests::beginCreatesRunningThread()
{
    ActionThreadTracker tracker;

    ActionThreadStartContext context;
    context.threadId = QStringLiteral("thread-01");
    context.taskType = QStringLiteral("web_search");
    context.userGoal = QStringLiteral("find docs");
    context.resultSummary = QStringLiteral("Working on it.");
    context.nextStepHint = QStringLiteral("Open the first result.");
    context.updatedAtMs = 100;
    context.expiresAtMs = 500;

    tracker.begin(context);

    QVERIFY(tracker.current().has_value());
    const ActionThread &thread = *tracker.current();
    QCOMPARE(thread.id, QStringLiteral("thread-01"));
    QCOMPARE(thread.taskType, QStringLiteral("web_search"));
    QCOMPARE(thread.userGoal, QStringLiteral("find docs"));
    QCOMPARE(thread.resultSummary, QStringLiteral("Working on it."));
    QCOMPARE(thread.nextStepHint, QStringLiteral("Open the first result."));
    QCOMPARE(thread.state, ActionThreadState::Running);
    QVERIFY(!thread.success);
    QVERIFY(thread.valid);
    QCOMPARE(thread.updatedAtMs, 100);
    QCOMPARE(thread.expiresAtMs, 500);
}

void ActionThreadTrackerTests::rememberResultUpdatesExistingThreadAndMapsSuccess()
{
    ActionThreadTracker tracker;

    ActionThreadStartContext beginContext;
    beginContext.threadId = QStringLiteral("thread-existing");
    beginContext.taskType = QStringLiteral("calendar");
    beginContext.userGoal = QStringLiteral("set reminder");
    beginContext.resultSummary = QStringLiteral("Working on it.");
    beginContext.updatedAtMs = 10;
    beginContext.expiresAtMs = 1000;
    tracker.begin(beginContext);

    ActionThreadResultContext resultContext;
    resultContext.taskType = QStringLiteral("   ");
    resultContext.resultSummary = QStringLiteral("Reminder created.");
    resultContext.artifactText = QStringLiteral("id=abc");
    resultContext.payload = QJsonObject{{QStringLiteral("id"), QStringLiteral("abc")}};
    resultContext.sourceUrls = {QStringLiteral("https://example.test")};
    resultContext.nextStepHint = QStringLiteral("Ask if they want another reminder.");
    resultContext.success = true;
    resultContext.taskState = TaskState::Finished;
    resultContext.updatedAtMs = 200;
    resultContext.expiresAtMs = 1200;

    tracker.rememberResult(resultContext);

    QVERIFY(tracker.current().has_value());
    const ActionThread &thread = *tracker.current();
    QCOMPARE(thread.id, QStringLiteral("thread-existing"));
    QCOMPARE(thread.taskType, QStringLiteral("calendar"));
    QCOMPARE(thread.userGoal, QStringLiteral("set reminder"));
    QCOMPARE(thread.resultSummary, QStringLiteral("Reminder created."));
    QCOMPARE(thread.artifactText, QStringLiteral("id=abc"));
    QCOMPARE(thread.payload.value(QStringLiteral("id")).toString(), QStringLiteral("abc"));
    QCOMPARE(thread.sourceUrls.size(), 1);
    QCOMPARE(thread.sourceUrls.front(), QStringLiteral("https://example.test"));
    QCOMPARE(thread.nextStepHint, QStringLiteral("Ask if they want another reminder."));
    QCOMPARE(thread.state, ActionThreadState::Completed);
    QVERIFY(thread.success);
    QVERIFY(thread.valid);
}

void ActionThreadTrackerTests::rememberResultMapsFailure()
{
    ActionThreadTracker tracker;

    ActionThreadResultContext context;
    context.fallbackThreadId = QStringLiteral("thread-fallback");
    context.fallbackUserGoal = QStringLiteral("summarize report");
    context.taskType = QStringLiteral("file_read");
    context.resultSummary = QStringLiteral("Permission denied.");
    context.success = false;
    context.taskState = TaskState::Finished;
    context.updatedAtMs = 300;
    context.expiresAtMs = 700;

    tracker.rememberResult(context);

    QVERIFY(tracker.current().has_value());
    const ActionThread &thread = *tracker.current();
    QCOMPARE(thread.id, QStringLiteral("thread-fallback"));
    QCOMPARE(thread.userGoal, QStringLiteral("summarize report"));
    QCOMPARE(thread.taskType, QStringLiteral("file_read"));
    QCOMPARE(thread.state, ActionThreadState::Failed);
    QVERIFY(!thread.success);
}

void ActionThreadTrackerTests::rememberResultMapsCanceled()
{
    ActionThreadTracker tracker;

    ActionThreadResultContext context;
    context.fallbackThreadId = QStringLiteral("thread-canceled");
    context.fallbackUserGoal = QStringLiteral("open browser");
    context.taskType = QStringLiteral("browser_open");
    context.resultSummary = QStringLiteral("Canceled by user.");
    context.success = false;
    context.taskState = TaskState::Canceled;
    context.updatedAtMs = 450;
    context.expiresAtMs = 900;

    tracker.rememberResult(context);

    QVERIFY(tracker.current().has_value());
    const ActionThread &thread = *tracker.current();
    QCOMPARE(thread.state, ActionThreadState::Canceled);
    QVERIFY(!thread.success);
}

void ActionThreadTrackerTests::rememberReplyCreatesCompletedAndFailedThread()
{
    ActionThreadTracker tracker;

    ActionThreadReplyContext successContext;
    successContext.threadId = QStringLiteral("reply-1");
    successContext.taskType = QStringLiteral("assistant_action");
    successContext.userGoal = QStringLiteral("draft message");
    successContext.resultSummary = QStringLiteral(" Draft created. ");
    successContext.nextStepHint = QStringLiteral("Review before sending.");
    successContext.success = true;
    successContext.updatedAtMs = 1000;
    successContext.expiresAtMs = 2000;
    tracker.rememberReply(successContext);

    QVERIFY(tracker.current().has_value());
    QCOMPARE(tracker.current()->state, ActionThreadState::Completed);
    QCOMPARE(tracker.current()->resultSummary, QStringLiteral("Draft created."));

    ActionThreadReplyContext failedContext;
    failedContext.threadId = QStringLiteral("reply-2");
    failedContext.taskType = QStringLiteral("assistant_action");
    failedContext.userGoal = QStringLiteral("send message");
    failedContext.resultSummary = QStringLiteral("Delivery failed");
    failedContext.nextStepHint = QStringLiteral("Try again later.");
    failedContext.success = false;
    failedContext.updatedAtMs = 2001;
    failedContext.expiresAtMs = 3000;
    tracker.rememberReply(failedContext);

    QVERIFY(tracker.current().has_value());
    QCOMPARE(tracker.current()->id, QStringLiteral("reply-2"));
    QCOMPARE(tracker.current()->state, ActionThreadState::Failed);
    QVERIFY(!tracker.current()->success);
}

void ActionThreadTrackerTests::buildContinuationInputPreservesLegacyPromptAndFallback()
{
    ActionThreadTracker tracker;

    const QString noThreadInput = QStringLiteral("what next?");
    QCOMPARE(tracker.buildContinuationInput(noThreadInput), noThreadInput);

    ActionThreadStartContext startContext;
    startContext.threadId = QStringLiteral("thread-legacy");
    startContext.taskType = QStringLiteral("task_type");
    startContext.userGoal = QStringLiteral("goal text");
    startContext.resultSummary = QStringLiteral("summary text");
    startContext.nextStepHint = QStringLiteral("next step text");
    startContext.updatedAtMs = 10;
    startContext.expiresAtMs = 2000;
    tracker.begin(startContext);

    ActionThreadResultContext resultContext;
    resultContext.taskType = QStringLiteral("task_type");
    resultContext.resultSummary = QStringLiteral("summary text");
    resultContext.artifactText = QStringLiteral("artifact text");
    resultContext.sourceUrls = {QStringLiteral("https://one"), QStringLiteral("https://two")};
    resultContext.nextStepHint = QStringLiteral("next step text");
    resultContext.success = true;
    resultContext.taskState = TaskState::Finished;
    resultContext.updatedAtMs = 11;
    resultContext.expiresAtMs = 2100;
    tracker.rememberResult(resultContext);

    const QString actual = tracker.buildContinuationInput(QStringLiteral("follow up question"));
    const QString expected = QStringLiteral(
        "You are continuing the current assistant action thread.\n"
        "Treat the user's message as a follow-up to this task when appropriate. "
        "Only start a brand-new unrelated task if the user clearly asks for one.\n\n"
        "Thread state: completed\n"
        "Task type: task_type\n"
        "User goal: goal text\n"
        "Result summary: summary text\n"
        "Artifacts:\nartifact text\n"
        "Sources:\nhttps://one\nhttps://two\n"
        "Suggested next step: next step text\n\n"
        "User follow-up: follow up question");
    QCOMPARE(actual, expected);
}

void ActionThreadTrackerTests::sanitizesRecursiveContinuationEnvelope()
{
    ActionThreadTracker tracker;

    const QString recursiveGoal = QStringLiteral(
        "You are continuing the current assistant action thread.\n"
        "Treat the user's message as a follow-up to this task when appropriate. "
        "Only start a brand-new unrelated task if the user clearly asks for one.\n\n"
        "Thread state: completed\n"
        "Task type: browser_open\n"
        "User goal: Open YouTube and search for machine learning courses\n"
        "Result summary: Opened search results\n\n"
        "User follow-up: Create a simple HTML snake game and launch it on the browser.");

    ActionThreadStartContext startContext;
    startContext.threadId = QStringLiteral("thread-recursive");
    startContext.taskType = QStringLiteral("browser_open");
    startContext.userGoal = recursiveGoal;
    startContext.resultSummary = recursiveGoal;
    startContext.nextStepHint = recursiveGoal;
    startContext.updatedAtMs = 10;
    startContext.expiresAtMs = 2000;
    tracker.begin(startContext);

    QVERIFY(tracker.current().has_value());
    QCOMPARE(tracker.current()->userGoal,
             QStringLiteral("Create a simple HTML snake game and launch it on the browser."));
    QVERIFY(!tracker.current()->resultSummary.contains(QStringLiteral("Thread state:")));
    QVERIFY(!tracker.current()->nextStepHint.contains(QStringLiteral("Thread state:")));

    const QString continuation = tracker.buildContinuationInput(QStringLiteral("what have you done?"));
    QCOMPARE(continuation.count(QStringLiteral("You are continuing the current assistant action thread.")), 1);
    QVERIFY(!continuation.contains(QStringLiteral("User goal: You are continuing")));
    QVERIFY(!continuation.contains(QStringLiteral("Result summary: You are continuing")));
    QVERIFY(continuation.contains(QStringLiteral("User goal: Create a simple HTML snake game")));
}

void ActionThreadTrackerTests::buildCompletionInputPreservesLegacyPromptAndFallback()
{
    ActionThreadTracker tracker;

    ActionThread thread;
    const QString fallbackOutput = tracker.buildCompletionInput(thread);
    const QString fallbackExpected = QStringLiteral(
        "A task just completed.\n"
        "Give the user a short useful completion summary grounded only in the task result below. "
        "If there is an obvious next step, suggest one short follow-up.\n\n"
        "Task type: task\n"
        "User goal: unknown\n"
        "Result summary: none\n"
        "Artifacts:\nnone\n"
        "Sources:\nnone\n"
        "Suggested next step: none");
    QCOMPARE(fallbackOutput, fallbackExpected);

    thread.taskType = QStringLiteral("web_search");
    thread.userGoal = QStringLiteral("find docs");
    thread.resultSummary = QStringLiteral("Found two sources");
    thread.artifactText = QStringLiteral("Top links listed");
    thread.sourceUrls = {QStringLiteral("https://alpha")};
    thread.nextStepHint = QStringLiteral("Open the first source");

    const QString output = tracker.buildCompletionInput(thread);
    const QString expected = QStringLiteral(
        "A task just completed.\n"
        "Give the user a short useful completion summary grounded only in the task result below. "
        "If there is an obvious next step, suggest one short follow-up.\n\n"
        "Task type: web_search\n"
        "User goal: find docs\n"
        "Result summary: Found two sources\n"
        "Artifacts:\nTop links listed\n"
        "Sources:\nhttps://alpha\n"
        "Suggested next step: Open the first source");
    QCOMPARE(output, expected);
}

void ActionThreadTrackerTests::clearRemovesCurrentThread()
{
    ActionThreadTracker tracker;

    ActionThreadStartContext context;
    context.threadId = QStringLiteral("clear-me");
    context.taskType = QStringLiteral("task");
    context.userGoal = QStringLiteral("goal");
    context.resultSummary = QStringLiteral("running");
    context.updatedAtMs = 5;
    context.expiresAtMs = 100;
    tracker.begin(context);

    QVERIFY(tracker.hasCurrent());
    tracker.clear();
    QVERIFY(!tracker.hasCurrent());
    QVERIFY(!tracker.current().has_value());
}

void ActionThreadTrackerTests::isCurrentUsableReturnsFalseForMissingOrExpiredThread()
{
    ActionThreadTracker tracker;

    QVERIFY(!tracker.isCurrentUsable(100));

    ActionThreadStartContext expiredContext;
    expiredContext.threadId = QStringLiteral("expired");
    expiredContext.taskType = QStringLiteral("task");
    expiredContext.userGoal = QStringLiteral("goal");
    expiredContext.resultSummary = QStringLiteral("done");
    expiredContext.updatedAtMs = 10;
    expiredContext.expiresAtMs = 20;
    tracker.begin(expiredContext);

    QVERIFY(!tracker.isCurrentUsable(30));
    QVERIFY(tracker.isCurrentUsable(15));
}

void ActionThreadTrackerTests::recentThreadsSurviveClearAndShortRestart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("action_threads.json"));

    ActionThreadTracker tracker;
    tracker.enablePersistence(path);

    ActionThreadReplyContext first;
    first.threadId = QStringLiteral("thread-one");
    first.taskType = QStringLiteral("browser_open");
    first.userGoal = QStringLiteral("Open the browser");
    first.resultSummary = QStringLiteral("Opened.");
    first.success = true;
    first.updatedAtMs = 100;
    first.expiresAtMs = 1000;
    tracker.rememberReply(first);

    ActionThreadReplyContext second;
    second.threadId = QStringLiteral("thread-two");
    second.taskType = QStringLiteral("file_write");
    second.userGoal = QStringLiteral("Create the game file");
    second.resultSummary = QStringLiteral("Created.");
    second.success = true;
    second.updatedAtMs = 200;
    second.expiresAtMs = 1000;
    tracker.rememberReply(second);

    QCOMPARE(tracker.recentThreads(250).size(), 2);
    tracker.clear();
    QVERIFY(!tracker.hasCurrent());
    QCOMPARE(tracker.recentThreads(250).size(), 2);

    ActionThreadTracker reloaded;
    reloaded.enablePersistence(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.recentThreads(250).size(), 2);
    QVERIFY(reloaded.threadById(QStringLiteral("thread-one")).has_value());
    QVERIFY(reloaded.threadById(QStringLiteral("thread-two")).has_value());
}

QTEST_MAIN(ActionThreadTrackerTests)

#include "ActionThreadTrackerTests.moc"
