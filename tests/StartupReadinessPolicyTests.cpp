#include <QtTest>

#include "core/StartupReadinessPolicy.h"

namespace {
StartupReadinessInput makeReadyInput()
{
    StartupReadinessInput input;
    input.initialSetupCompleted = true;
    input.chatBackendEndpoint = QStringLiteral("http://127.0.0.1:1234");
    input.whisperExecutable = QStringLiteral("whisper.exe");
    input.whisperExecutableExists = true;
    input.whisperModelPath = QStringLiteral("whisper-model.bin");
    input.whisperModelExists = true;
    input.piperExecutable = QStringLiteral("piper.exe");
    input.piperExecutableExists = true;
    input.piperVoiceModel = QStringLiteral("voice.onnx");
    input.piperVoiceModelExists = true;
    input.ffmpegExecutable = QStringLiteral("ffmpeg.exe");
    input.ffmpegExecutableExists = true;
    input.wakeEngineRequired = false;
    input.modelCatalogResolved = true;
    input.availability.online = true;
    input.availability.modelAvailable = true;
    input.wakeStartRequested = true;
    input.wakeEngineReady = true;
    return input;
}
}

class StartupReadinessPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void initialSetupIncomplete();
    void missingBackendEndpoint();
    void missingWhisperExecutable();
    void missingWhisperModel();
    void missingPiperExecutable();
    void missingPiperVoiceModel();
    void missingFfmpegExecutable();
    void wakeRuntimeMissingWhenRequired();
    void wakeModelMissingWhenRequired();
    void modelCatalogStillLoading();
    void aiBackendOffline();
    void selectedModelUnavailable();
    void wakeEngineStillStarting();
    void wakeEngineError();
    void readyState();
};

void StartupReadinessPolicyTests::initialSetupIncomplete()
{
    StartupReadinessInput input = makeReadyInput();
    input.initialSetupCompleted = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Initial setup is incomplete."));
}

void StartupReadinessPolicyTests::missingBackendEndpoint()
{
    StartupReadinessInput input = makeReadyInput();
    input.chatBackendEndpoint = QStringLiteral("   ");

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Local AI backend endpoint is missing."));
}

void StartupReadinessPolicyTests::missingWhisperExecutable()
{
    StartupReadinessInput input = makeReadyInput();
    input.whisperExecutable.clear();
    input.whisperExecutableExists = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Whisper executable is missing."));
}

void StartupReadinessPolicyTests::missingWhisperModel()
{
    StartupReadinessInput input = makeReadyInput();
    input.whisperModelPath.clear();
    input.whisperModelExists = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Whisper model is missing."));
}

void StartupReadinessPolicyTests::missingPiperExecutable()
{
    StartupReadinessInput input = makeReadyInput();
    input.piperExecutable.clear();
    input.piperExecutableExists = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Piper executable is missing."));
}

void StartupReadinessPolicyTests::missingPiperVoiceModel()
{
    StartupReadinessInput input = makeReadyInput();
    input.piperVoiceModel.clear();
    input.piperVoiceModelExists = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Piper voice model is missing."));
}

void StartupReadinessPolicyTests::missingFfmpegExecutable()
{
    StartupReadinessInput input = makeReadyInput();
    input.ffmpegExecutable.clear();
    input.ffmpegExecutableExists = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("FFmpeg executable is missing."));
}

void StartupReadinessPolicyTests::wakeRuntimeMissingWhenRequired()
{
    StartupReadinessInput input = makeReadyInput();
    input.wakeEngineRequired = true;
    input.wakeRuntimePath.clear();
    input.wakeModelPath = QStringLiteral("wake-model.onnx");

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Wake runtime is missing."));
}

void StartupReadinessPolicyTests::wakeModelMissingWhenRequired()
{
    StartupReadinessInput input = makeReadyInput();
    input.wakeEngineRequired = true;
    input.wakeRuntimePath = QStringLiteral("wake-runtime.dll");
    input.wakeModelPath.clear();

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Wake model is missing."));
}

void StartupReadinessPolicyTests::modelCatalogStillLoading()
{
    StartupReadinessInput input = makeReadyInput();
    input.modelCatalogResolved = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(!result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Loading local AI backend..."));
}

void StartupReadinessPolicyTests::aiBackendOffline()
{
    StartupReadinessInput input = makeReadyInput();
    input.availability.online = false;
    input.availability.modelAvailable = false;
    input.availability.status.clear();

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Local AI backend is offline."));
}

void StartupReadinessPolicyTests::selectedModelUnavailable()
{
    StartupReadinessInput input = makeReadyInput();
    input.availability.online = true;
    input.availability.modelAvailable = false;
    input.availability.status.clear();

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Selected model is unavailable."));
}

void StartupReadinessPolicyTests::wakeEngineStillStarting()
{
    StartupReadinessInput input = makeReadyInput();
    input.wakeEngineRequired = true;
    input.wakeRuntimePath = QStringLiteral("wake-runtime.dll");
    input.wakeModelPath = QStringLiteral("wake-model.onnx");
    input.wakeStartRequested = true;
    input.wakeEngineReady = false;

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(!result.blocked);
    QCOMPARE(result.issue, QStringLiteral("Starting wake engine..."));
}

void StartupReadinessPolicyTests::wakeEngineError()
{
    StartupReadinessInput input = makeReadyInput();
    input.wakeEngineRequired = true;
    input.wakeRuntimePath = QStringLiteral("wake-runtime.dll");
    input.wakeModelPath = QStringLiteral("wake-model.onnx");
    input.wakeStartRequested = true;
    input.wakeEngineReady = false;
    input.wakeEngineError = QStringLiteral("wake engine failed");

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(!result.ready);
    QVERIFY(result.blocked);
    QCOMPARE(result.issue, QStringLiteral("wake engine failed"));
}

void StartupReadinessPolicyTests::readyState()
{
    const StartupReadinessInput input = makeReadyInput();

    const StartupReadinessResult result = StartupReadinessPolicy::evaluate(input);

    QVERIFY(result.ready);
    QVERIFY(!result.blocked);
    QVERIFY(result.issue.isEmpty());
}

QTEST_APPLESS_MAIN(StartupReadinessPolicyTests)
#include "StartupReadinessPolicyTests.moc"
