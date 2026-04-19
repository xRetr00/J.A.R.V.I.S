#include <QtTest>
#include <QJsonArray>

#include "core/ToolCoordinator.h"
#include "core/tools/ToolExecutionService.h"
#include "core/tools/ToolResultEvidencePolicy.h"

class ToolCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void webSearchFollowUpUsesTextPayloadWhenContentMissing();
    void webSearchFollowUpUsesSourcesWhenTextMissing();
    void webSearchFollowUpUsesJsonOnlyPayload();
    void jsonOnlyWebSearchPayloadProducesModelOutput();
    void emptyBrowserFetchTextIsLowSignal();
    void rawHtmlSearchShellIsWeakEvidence();
};

void ToolCoordinatorTests::webSearchFollowUpUsesTextPayloadWhenContentMissing()
{
    ToolCoordinator coordinator;

    BackgroundTaskResult result;
    result.type = QStringLiteral("web_search");
    result.success = true;
    result.summary = QStringLiteral("Tesla latest news");
    result.payload = QJsonObject{
        {QStringLiteral("query"), QStringLiteral("latest news about Tesla")},
        {QStringLiteral("provider"), QStringLiteral("duckduckgo")},
        {QStringLiteral("text"), QStringLiteral("Tesla released a new quarterly update.")}
    };

    const auto followUp = coordinator.buildWebSearchFollowUp(result);
    QVERIFY(followUp.has_value());
    QVERIFY(!followUp->synthesisInput.isEmpty());
    QVERIFY(followUp->synthesisInput.contains(QStringLiteral("Tesla released a new quarterly update.")));
}

void ToolCoordinatorTests::webSearchFollowUpUsesSourcesWhenTextMissing()
{
    ToolCoordinator coordinator;

    BackgroundTaskResult result;
    result.type = QStringLiteral("web_search");
    result.success = true;
    result.payload = QJsonObject{
        {QStringLiteral("query"), QStringLiteral("latest Vaxil update")},
        {QStringLiteral("sources"), QJsonArray{
            QJsonObject{
                {QStringLiteral("title"), QStringLiteral("Vaxil changelog")},
                {QStringLiteral("url"), QStringLiteral("https://example.com/changelog")},
                {QStringLiteral("snippet"), QStringLiteral("The latest update improved background task follow-up handling.")}
            }
        }}
    };

    const auto followUp = coordinator.buildWebSearchFollowUp(result);
    QVERIFY(followUp.has_value());
    QVERIFY(!followUp->synthesisInput.isEmpty());
    QVERIFY(followUp->synthesisInput.contains(QStringLiteral("https://example.com/changelog")));
}

void ToolCoordinatorTests::webSearchFollowUpUsesJsonOnlyPayload()
{
    ToolCoordinator coordinator;

    BackgroundTaskResult result;
    result.type = QStringLiteral("web_search");
    result.success = true;
    result.payload = QJsonObject{
        {QStringLiteral("query"), QStringLiteral("machine learning courses")},
        {QStringLiteral("provider"), QStringLiteral("brave")},
        {QStringLiteral("json"), QStringLiteral("{\"web\":{\"results\":[{\"title\":\"Machine Learning Course\",\"url\":\"https://example.com/ml\",\"description\":\"A practical course.\"}]}}")}
    };

    const auto followUp = coordinator.buildWebSearchFollowUp(result);
    QVERIFY(followUp.has_value());
    QVERIFY(followUp->synthesisInput.contains(QStringLiteral("https://example.com/ml")));
}

void ToolCoordinatorTests::jsonOnlyWebSearchPayloadProducesModelOutput()
{
    ToolExecutionResult result;
    result.toolName = QStringLiteral("web_search");
    result.success = true;
    result.payload = QJsonObject{
        {QStringLiteral("json"), QStringLiteral("{\"web\":{\"results\":[{\"title\":\"Course result\",\"url\":\"https://example.com/course\",\"description\":\"A grounded source.\"}]}}")}
    };

    const QString output = ToolExecutionService::outputTextForModel(result);
    QVERIFY(output.contains(QStringLiteral("Course result")));
    QVERIFY(output.contains(QStringLiteral("https://example.com/course")));
}

void ToolCoordinatorTests::emptyBrowserFetchTextIsLowSignal()
{
    ToolExecutionResult result;
    result.toolName = QStringLiteral("browser_fetch_text");
    result.success = true;
    result.detail = QStringLiteral("Fetched and extracted text from https://youtube.com with Playwright.");
    result.payload = QJsonObject{
        {QStringLiteral("url"), QStringLiteral("https://youtube.com")},
        {QStringLiteral("text"), QString()}
    };

    const ToolResultEvidenceAssessment assessment =
        ToolResultEvidencePolicy::assess(result, ToolExecutionService::outputTextForModel(result));
    QVERIFY(assessment.lowSignal);
    QCOMPARE(assessment.lowSignalReason, QStringLiteral("tool_result.empty_browser_text"));
}

void ToolCoordinatorTests::rawHtmlSearchShellIsWeakEvidence()
{
    ToolExecutionResult result;
    result.toolName = QStringLiteral("web_fetch");
    result.success = true;
    result.payload = QJsonObject{
        {QStringLiteral("content"), QStringLiteral("<html><body><script>window.x=1</script>enable javascript to continue</body></html>")}
    };

    const ToolResultEvidenceAssessment assessment =
        ToolResultEvidencePolicy::assess(result, QStringLiteral("<html><script>search?q=avengers</script></html>"));
    QVERIFY(assessment.lowSignal);
    QCOMPARE(assessment.lowSignalReason, QStringLiteral("tool_result.raw_search_or_browser_html"));
    QCOMPARE(assessment.confidence, QStringLiteral("weak"));
}

QTEST_APPLESS_MAIN(ToolCoordinatorTests)
#include "ToolCoordinatorTests.moc"
