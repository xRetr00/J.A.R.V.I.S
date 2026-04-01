#include <QtTest>
#include <QJsonArray>

#include "core/ToolCoordinator.h"

class ToolCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void webSearchFollowUpUsesTextPayloadWhenContentMissing();
    void webSearchFollowUpUsesSourcesWhenTextMissing();
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

QTEST_APPLESS_MAIN(ToolCoordinatorTests)
#include "ToolCoordinatorTests.moc"
