#include <QtTest>

#include <QFile>
#include <QJsonDocument>

#include "core/intent/IntentTuningConfig.h"
#include "core/intent/RoutingCalibrationAnalyzer.h"
#include "core/intent/RoutingReplayHarness.h"
#include "core/intent/RoutingTraceEmitter.h"

class IntentIntelligenceTuningTests : public QObject
{
    Q_OBJECT

private slots:
    void summarizesCalibrationTelemetry();
    void replaysRoutingFixtures();
    void emitsAdvisorModeAndEvaluationInTrace();
    void thresholdConfigExposesStableDefaults();
};

void IntentIntelligenceTuningTests::summarizesCalibrationTelemetry()
{
    RoutingCalibrationAnalyzer analyzer;
    const QStringList lines = {
        QStringLiteral(R"({"record":"route_final","final_executed_route":"conversation","ambiguity_score":0.71,"intent_confidence":{"final":0.41},"arbitrator_reason_codes":["arbitrator.backend_escalation"],"overrides":[]})"),
        QStringLiteral(R"({"record":"route_final","final_executed_route":"local_response","ambiguity_score":0.82,"intent_confidence":{"final":0.35},"arbitrator_reason_codes":["arbitrator.ask_clarification"],"overrides":[]})"),
        QStringLiteral(R"({"record":"route_final","final_executed_route":"deterministic_tasks","ambiguity_score":0.10,"intent_confidence":{"final":0.92},"arbitrator_reason_codes":["arbitrator.deterministic_priority"],"overrides":[]})")
    };

    const QList<QJsonObject> records = analyzer.parseRouteFinalRecords(lines);
    QCOMPARE(records.size(), 3);
    const RoutingCalibrationSummary summary = analyzer.summarize(records);
    QCOMPARE(summary.total, 3);
    QVERIFY(summary.meanConfidence > 0.5f);
    QVERIFY(summary.backendEscalationFrequency > 0.0f);
    QVERIFY(summary.clarificationFrequency > 0.0f);
    QVERIFY(!summary.thresholdObservations.isEmpty());
}

void IntentIntelligenceTuningTests::replaysRoutingFixtures()
{
    const QString fixturePath = QFINDTESTDATA("fixtures/intent_routing_replay_fixtures.json");
    QVERIFY2(!fixturePath.isEmpty(), "fixture file not found");

    QFile fixtureFile(fixturePath);
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(fixtureFile.readAll());
    QVERIFY(document.isArray());

    RoutingReplayHarness harness;
    const QList<RoutingReplayFixture> fixtures = harness.fixturesFromJsonArray(document.array());
    QVERIFY(fixtures.size() >= 10);

    for (const RoutingReplayFixture &fixture : fixtures) {
        const RoutingReplayResult result = harness.replay(fixture);
        if (fixture.expectedFinalRoute != InputRouteKind::None) {
            QCOMPARE(result.arbitration.decision.kind, fixture.expectedFinalRoute);
        }
        if (fixture.expectedTopCandidateRoute != InputRouteKind::None) {
            QVERIFY(!result.candidates.isEmpty());
            QCOMPARE(result.candidates.first().route.kind, fixture.expectedTopCandidateRoute);
        }
        if (fixture.expectedClarification) {
            QVERIFY(result.arbitration.reasonCodes.contains(QStringLiteral("arbitrator.ask_clarification")));
        }
        if (fixture.expectedBackendEscalation) {
            QVERIFY(result.arbitration.reasonCodes.contains(QStringLiteral("arbitrator.backend_escalation"))
                    || result.arbitration.reasonCodes.contains(QStringLiteral("arbitrator.backend_escalation_fallback")));
        }
        for (const QString &requiredReason : fixture.requiredReasonCodes) {
            QVERIFY2(result.arbitration.reasonCodes.contains(requiredReason), qPrintable(fixture.name));
        }
    }
}

void IntentIntelligenceTuningTests::emitsAdvisorModeAndEvaluationInTrace()
{
    RoutingTrace trace;
    trace.rawInput = QStringLiteral("open or explain this");
    trace.normalizedInput = trace.rawInput;
    trace.advisorMode = IntentAdvisorMode::ShadowLearned;
    trace.advisorSuggestion = IntentAdvisorSuggestion{
        .available = false,
        .ambiguityBoost = 0.25f,
        .continuationLikelihood = 0.2f,
        .backendNecessity = 0.71f,
        .reasonCodes = {QStringLiteral("advisor.mode.shadow_learned")}
    };
    trace.advisorEvaluation.baseAmbiguity = 0.55f;
    trace.advisorEvaluation.adjustedAmbiguity = 0.8f;
    trace.advisorEvaluation.ambiguityPreferenceChanged = true;
    trace.advisorEvaluation.baseBackendPreference = 0.4f;
    trace.advisorEvaluation.adjustedBackendPreference = 0.71f;
    trace.advisorEvaluation.backendPreferenceChanged = true;
    trace.advisorEvaluation.reasonCodes = {QStringLiteral("advisor_eval.shadow_compare_enabled")};
    trace.finalExecutedRoute = QStringLiteral("conversation");

    RoutingTraceEmitter emitter;
    const QJsonObject payload = emitter.buildRouteFinalPayload(trace);
    QCOMPARE(payload.value(QStringLiteral("advisor_mode")).toString(), QStringLiteral("shadow_learned"));
    QVERIFY(payload.contains(QStringLiteral("advisor_evaluation")));
    const QJsonObject advisorEval = payload.value(QStringLiteral("advisor_evaluation")).toObject();
    QVERIFY(advisorEval.value(QStringLiteral("ambiguity_preference_changed")).toBool());
    QVERIFY(advisorEval.value(QStringLiteral("backend_preference_changed")).toBool());
}

void IntentIntelligenceTuningTests::thresholdConfigExposesStableDefaults()
{
    const IntentTuningThresholds &thresholds = IntentTuningConfig::thresholds();
    QVERIFY(thresholds.highAmbiguity > 0.0f);
    QVERIFY(thresholds.mediumConfidence > thresholds.lowConfidence);
    QCOMPARE(IntentTuningConfig::advisorModeToString(IntentAdvisorMode::Heuristic), QStringLiteral("heuristic"));
}

QTEST_APPLESS_MAIN(IntentIntelligenceTuningTests)
#include "IntentIntelligenceTuningTests.moc"

