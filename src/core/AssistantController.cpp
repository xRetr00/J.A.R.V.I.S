#include "core/AssistantController.h"

#include <QState>

#include <nlohmann/json.hpp>

#include "ai/LmStudioClient.h"
#include "ai/ModelCatalogService.h"
#include "ai/PromptAdapter.h"
#include "ai/ReasoningRouter.h"
#include "ai/StreamAssembler.h"
#include "audio/AudioInputService.h"
#include "devices/DeviceManager.h"
#include "logging/LoggingService.h"
#include "memory/MemoryStore.h"
#include "settings/AppSettings.h"
#include "stt/WhisperSttEngine.h"
#include "tts/PiperTtsEngine.h"

namespace {
QString stateToString(AssistantState state)
{
    switch (state) {
    case AssistantState::Idle:
        return QStringLiteral("IDLE");
    case AssistantState::Listening:
        return QStringLiteral("LISTENING");
    case AssistantState::Processing:
        return QStringLiteral("PROCESSING");
    case AssistantState::Speaking:
        return QStringLiteral("SPEAKING");
    }
    return QStringLiteral("IDLE");
}
}

AssistantController::AssistantController(AppSettings *settings, LoggingService *loggingService, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_loggingService(loggingService)
{
    m_lmStudioClient = new LmStudioClient(this);
    m_modelCatalogService = new ModelCatalogService(m_settings, m_lmStudioClient, this);
    m_reasoningRouter = new ReasoningRouter(this);
    m_promptAdapter = new PromptAdapter(this);
    m_streamAssembler = new StreamAssembler(this);
    m_memoryStore = new MemoryStore(this);
    m_deviceManager = new DeviceManager(this);
    m_audioInputService = new AudioInputService(this);
    m_whisperSttEngine = new WhisperSttEngine(m_settings, this);
    m_piperTtsEngine = new PiperTtsEngine(m_settings, this);
}

void AssistantController::initialize()
{
    m_lmStudioClient->setEndpoint(m_settings->lmStudioEndpoint());
    m_deviceManager->registerDefaults();
    setupStateMachine();
    refreshModels();

    connect(m_modelCatalogService, &ModelCatalogService::modelsChanged, this, &AssistantController::modelsChanged);
    connect(m_modelCatalogService, &ModelCatalogService::availabilityChanged, this, [this]() {
        setStatus(m_modelCatalogService->availability().status);
    });

    connect(m_audioInputService, &AudioInputService::audioLevelChanged, this, [this](const AudioLevel &level) {
        m_audioLevel = level.rms;
        emit audioLevelChanged();
    });
    connect(m_audioInputService, &AudioInputService::speechEnded, this, &AssistantController::stopListening);

    connect(m_whisperSttEngine, &WhisperSttEngine::transcriptionReady, this, [this](const TranscriptionResult &result) {
        m_transcript = result.text;
        emit transcriptChanged();
        if (result.text.isEmpty()) {
            setStatus(QStringLiteral("No speech detected"));
            emit idleRequested();
            return;
        }
        submitText(result.text);
    });
    connect(m_whisperSttEngine, &WhisperSttEngine::transcriptionFailed, this, [this](const QString &errorText) {
        setStatus(errorText);
        emit idleRequested();
    });

    connect(m_streamAssembler, &StreamAssembler::partialTextUpdated, this, [this](const QString &text) {
        m_responseText = text;
        emit responseTextChanged();
    });
    connect(m_streamAssembler, &StreamAssembler::sentenceReady, m_piperTtsEngine, &PiperTtsEngine::enqueueSentence);

    connect(m_piperTtsEngine, &PiperTtsEngine::playbackStarted, this, [this]() {
        emit speakingRequested();
    });
    connect(m_piperTtsEngine, &PiperTtsEngine::playbackFinished, this, [this]() {
        emit idleRequested();
    });
    connect(m_piperTtsEngine, &PiperTtsEngine::playbackFailed, this, [this](const QString &errorText) {
        setStatus(errorText);
        emit idleRequested();
    });

    connect(m_lmStudioClient, &LmStudioClient::requestStarted, this, [this](quint64 requestId) {
        m_activeRequestId = requestId;
        emit processingRequested();
    });
    connect(m_lmStudioClient, &LmStudioClient::requestDelta, this, [this](quint64 requestId, const QString &delta) {
        if (requestId == m_activeRequestId && m_activeRequestKind == RequestKind::Conversation) {
            m_streamAssembler->appendChunk(delta);
        }
    });
    connect(m_lmStudioClient, &LmStudioClient::requestFinished, this, [this](quint64 requestId, const QString &fullText) {
        if (requestId != m_activeRequestId) {
            return;
        }

        if (m_activeRequestKind == RequestKind::CommandExtraction) {
            handleCommandFinished(fullText);
        } else {
            handleConversationFinished(fullText);
        }
    });
    connect(m_lmStudioClient, &LmStudioClient::requestFailed, this, [this](quint64 requestId, const QString &errorText) {
        if (requestId == m_activeRequestId) {
            setStatus(errorText);
            emit idleRequested();
        }
    });
}

QString AssistantController::stateName() const { return stateToString(m_currentState); }
QString AssistantController::transcript() const { return m_transcript; }
QString AssistantController::responseText() const { return m_responseText; }
QString AssistantController::statusText() const { return m_statusText; }
float AssistantController::audioLevel() const { return m_audioLevel; }
QList<ModelInfo> AssistantController::availableModels() const { return m_modelCatalogService->models(); }
QStringList AssistantController::availableModelIds() const
{
    QStringList ids;
    for (const auto &model : availableModels()) {
        ids.push_back(model.id);
    }
    return ids;
}
QString AssistantController::selectedModel() const { return m_settings->selectedModel(); }

void AssistantController::refreshModels()
{
    m_lmStudioClient->setEndpoint(m_settings->lmStudioEndpoint());
    m_modelCatalogService->refresh();
}

void AssistantController::submitText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    m_transcript = trimmed;
    m_responseText.clear();
    m_streamAssembler->reset();
    m_piperTtsEngine->clear();
    emit transcriptChanged();
    emit responseTextChanged();
    setStatus(QStringLiteral("Processing request"));
    m_memoryStore->appendConversation(QStringLiteral("user"), trimmed);

    if (m_reasoningRouter->isLikelyCommand(trimmed)) {
        startCommandRequest(trimmed);
    } else {
        startConversationRequest(trimmed);
    }
}

void AssistantController::startListening()
{
    if (m_audioInputService->start(m_settings->micSensitivity())) {
        setStatus(QStringLiteral("Listening"));
        emit listeningRequested();
    } else {
        setStatus(QStringLiteral("No microphone available"));
    }
}

void AssistantController::stopListening()
{
    const QByteArray pcm = m_audioInputService->recordedPcm();
    m_audioInputService->stop();
    emit processingRequested();
    if (pcm.isEmpty()) {
        setStatus(QStringLiteral("No audio captured"));
        emit idleRequested();
        return;
    }
    m_whisperSttEngine->transcribePcm(pcm);
}

void AssistantController::cancelActiveRequest()
{
    m_lmStudioClient->cancelActiveRequest();
    m_piperTtsEngine->clear();
    setStatus(QStringLiteral("Request cancelled"));
    emit idleRequested();
}

void AssistantController::setSelectedModel(const QString &modelId)
{
    m_settings->setSelectedModel(modelId);
    m_settings->save();
    emit modelsChanged();
    refreshModels();
}

void AssistantController::saveSettings(
    const QString &endpoint,
    const QString &modelId,
    int defaultMode,
    bool autoRouting,
    bool streaming,
    int timeoutMs,
    const QString &whisperPath,
    const QString &piperPath,
    const QString &voicePath,
    const QString &ffmpegPath,
    double voiceSpeed,
    double voicePitch,
    double micSensitivity,
    bool clickThrough)
{
    m_settings->setLmStudioEndpoint(endpoint);
    m_settings->setSelectedModel(modelId);
    m_settings->setDefaultReasoningMode(static_cast<ReasoningMode>(defaultMode));
    m_settings->setAutoRoutingEnabled(autoRouting);
    m_settings->setStreamingEnabled(streaming);
    m_settings->setRequestTimeoutMs(timeoutMs);
    m_settings->setWhisperExecutable(whisperPath);
    m_settings->setPiperExecutable(piperPath);
    m_settings->setPiperVoiceModel(voicePath);
    m_settings->setFfmpegExecutable(ffmpegPath);
    m_settings->setVoiceSpeed(voiceSpeed);
    m_settings->setVoicePitch(voicePitch);
    m_settings->setMicSensitivity(micSensitivity);
    m_settings->setClickThroughEnabled(clickThrough);
    m_settings->save();
    refreshModels();
    setStatus(QStringLiteral("Settings saved"));
}

void AssistantController::setupStateMachine()
{
    auto *idle = new QState(&m_stateMachine);
    auto *listening = new QState(&m_stateMachine);
    auto *processing = new QState(&m_stateMachine);
    auto *speaking = new QState(&m_stateMachine);

    idle->addTransition(this, &AssistantController::listeningRequested, listening);
    idle->addTransition(this, &AssistantController::processingRequested, processing);
    listening->addTransition(this, &AssistantController::processingRequested, processing);
    listening->addTransition(this, &AssistantController::idleRequested, idle);
    processing->addTransition(this, &AssistantController::speakingRequested, speaking);
    processing->addTransition(this, &AssistantController::idleRequested, idle);
    speaking->addTransition(this, &AssistantController::idleRequested, idle);
    speaking->addTransition(this, &AssistantController::processingRequested, processing);

    connect(idle, &QState::entered, this, [this]() { m_currentState = AssistantState::Idle; emit stateChanged(); });
    connect(listening, &QState::entered, this, [this]() { m_currentState = AssistantState::Listening; emit stateChanged(); });
    connect(processing, &QState::entered, this, [this]() { m_currentState = AssistantState::Processing; emit stateChanged(); });
    connect(speaking, &QState::entered, this, [this]() { m_currentState = AssistantState::Speaking; emit stateChanged(); });

    m_stateMachine.setInitialState(idle);
    m_stateMachine.start();
}

void AssistantController::setStatus(const QString &status)
{
    m_statusText = status;
    if (m_loggingService) {
        m_loggingService->info(status);
    }
    emit statusTextChanged();
}

void AssistantController::startConversationRequest(const QString &input)
{
    const ReasoningMode mode = m_reasoningRouter->chooseMode(input, m_settings->autoRoutingEnabled(), m_settings->defaultReasoningMode());
    const QString modelId = m_settings->selectedModel().isEmpty() && !availableModelIds().isEmpty() ? availableModelIds().first() : m_settings->selectedModel();
    if (modelId.isEmpty()) {
        setStatus(QStringLiteral("No LM Studio model selected"));
        emit idleRequested();
        return;
    }

    m_activeRequestKind = RequestKind::Conversation;
    const auto messages = m_promptAdapter->buildConversationMessages(
        input,
        m_memoryStore->recentMessages(8),
        m_memoryStore->relevantMemory(input),
        mode);

    m_activeRequestId = m_lmStudioClient->sendChatRequest(messages, modelId, {
        .mode = mode,
        .kind = RequestKind::Conversation,
        .stream = m_settings->streamingEnabled(),
        .timeout = std::chrono::milliseconds(m_settings->requestTimeoutMs())
    });
}

void AssistantController::startCommandRequest(const QString &input)
{
    const QString modelId = m_settings->selectedModel().isEmpty() && !availableModelIds().isEmpty() ? availableModelIds().first() : m_settings->selectedModel();
    if (modelId.isEmpty()) {
        setStatus(QStringLiteral("No LM Studio model selected"));
        emit idleRequested();
        return;
    }

    m_activeRequestKind = RequestKind::CommandExtraction;
    m_activeRequestId = m_lmStudioClient->sendChatRequest(
        m_promptAdapter->buildCommandMessages(input, ReasoningMode::Fast),
        modelId,
        {.mode = ReasoningMode::Fast, .kind = RequestKind::CommandExtraction, .stream = false, .timeout = std::chrono::milliseconds(m_settings->requestTimeoutMs())});
}

void AssistantController::handleConversationFinished(const QString &text)
{
    m_responseText = text;
    emit responseTextChanged();
    m_memoryStore->appendConversation(QStringLiteral("assistant"), text);
    const QString remainder = m_streamAssembler->drainRemainingText();
    if (!remainder.isEmpty()) {
        m_piperTtsEngine->enqueueSentence(remainder);
    }
    if (!m_settings->streamingEnabled()) {
        m_piperTtsEngine->enqueueSentence(text);
    } else if (!m_piperTtsEngine->isSpeaking()) {
        emit idleRequested();
    }
    setStatus(QStringLiteral("Response ready"));
}

void AssistantController::handleCommandFinished(const QString &text)
{
    const CommandEnvelope command = parseCommand(text);
    if (!command.valid || command.confidence < 0.6f) {
        startConversationRequest(m_transcript);
        return;
    }

    const QString result = m_deviceManager->execute(command);
    m_responseText = result;
    emit responseTextChanged();
    m_memoryStore->appendConversation(QStringLiteral("assistant"), result);
    m_piperTtsEngine->enqueueSentence(result);
    setStatus(QStringLiteral("Command executed"));
}

CommandEnvelope AssistantController::parseCommand(const QString &payload) const
{
    const auto json = nlohmann::json::parse(payload.toStdString(), nullptr, false);
    if (json.is_discarded()) {
        return {};
    }

    CommandEnvelope command;
    command.intent = QString::fromStdString(json.value("intent", std::string{}));
    command.target = QString::fromStdString(json.value("target", std::string{}));
    command.action = QString::fromStdString(json.value("action", std::string{}));
    command.confidence = json.value("confidence", 0.0f);
    command.args = json.contains("args") ? json.at("args") : nlohmann::json::object();
    command.valid = !command.intent.isEmpty() && command.intent != QStringLiteral("unknown");
    return command;
}
