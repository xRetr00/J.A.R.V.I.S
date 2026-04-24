#include "ai/OpenAiCompatibleClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QScopeGuard>
#include <QDebug>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>
#include <optional>

namespace {
QString normalizeEndpoint(QString endpoint, const QString &providerKind)
{
    endpoint = endpoint.trimmed();
    if (endpoint.isEmpty()) {
        if (providerKind == QStringLiteral("openrouter")) {
            return QStringLiteral("https://openrouter.ai/api");
        }
        if (providerKind == QStringLiteral("ollama")) {
            return QStringLiteral("http://localhost:11434");
        }
        return QStringLiteral("http://localhost:1234");
    }

    while (endpoint.endsWith('/')) {
        endpoint.chop(1);
    }

    if (endpoint.endsWith(QStringLiteral("/v1"), Qt::CaseInsensitive)) {
        endpoint.chop(3);
        while (endpoint.endsWith('/')) {
            endpoint.chop(1);
        }
    }

    return endpoint;
}

QJsonObject functionToolSchema(const AgentToolSpec &tool)
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("function")},
        {QStringLiteral("name"), tool.name},
        {QStringLiteral("description"), tool.description},
        {QStringLiteral("parameters"), QJsonDocument::fromJson(QByteArray::fromStdString(tool.parameters.dump())).object()}
    };
}

QString normalizedProviderKind(const QString &providerKind)
{
    const QString normalized = providerKind.trimmed().toLower();
    return normalized.isEmpty() ? QStringLiteral("openai_compatible_local") : normalized;
}

bool shouldIncludeReasoningObject(const QString &providerKind)
{
    const QString normalized = normalizedProviderKind(providerKind);
    return normalized == QStringLiteral("openrouter")
        || normalized == QStringLiteral("openai")
        || normalized == QStringLiteral("azure_openai");
}

bool containsPromptLeakMarkers(const QString &text)
{
    const QString lowered = text.toLower();
    return lowered.contains(QStringLiteral("<identity>"))
        || lowered.contains(QStringLiteral("<behavior_mode>"))
        || lowered.contains(QStringLiteral("<response_contract>"))
        || lowered.contains(QStringLiteral("</user_follow_up>"))
        || lowered.contains(QStringLiteral("session goal:"));
}

QString firstNonEmptyTextField(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isString()) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        } else if (value.isArray()) {
            QStringList joined;
            const QJsonArray array = value.toArray();
            for (const QJsonValue &item : array) {
                if (item.isString()) {
                    const QString text = item.toString().trimmed();
                    if (!text.isEmpty()) {
                        joined.push_back(text);
                    }
                } else if (item.isObject()) {
                    const QString text = firstNonEmptyTextField(
                        item.toObject(),
                        {QStringLiteral("text"), QStringLiteral("content"), QStringLiteral("value")});
                    if (!text.isEmpty()) {
                        joined.push_back(text);
                    }
                }
            }
            if (!joined.isEmpty()) {
                return joined.join(QStringLiteral(" "));
            }
        } else if (value.isObject()) {
            const QString text = firstNonEmptyTextField(
                value.toObject(),
                {QStringLiteral("text"), QStringLiteral("content"), QStringLiteral("value"), QStringLiteral("output_text")});
            if (!text.isEmpty()) {
                return text;
            }
        }
    }
    return {};
}

std::optional<double> toDoubleIfNumeric(const QJsonValue &value)
{
    if (value.isDouble()) {
        return value.toDouble();
    }
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        if (ok) {
            return parsed;
        }
    }
    return std::nullopt;
}

std::optional<int> toIntIfNumeric(const QJsonValue &value)
{
    if (value.isDouble()) {
        return static_cast<int>(value.toDouble());
    }
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        if (ok) {
            return parsed;
        }
    }
    return std::nullopt;
}

void copyNumericField(const QJsonObject &source, const QString &sourceKey, QVariantMap *target, const QString &targetKey = QString())
{
    if (target == nullptr || !source.contains(sourceKey)) {
        return;
    }
    const QString finalKey = targetKey.isEmpty() ? sourceKey : targetKey;
    if (const std::optional<int> asInt = toIntIfNumeric(source.value(sourceKey))) {
        target->insert(finalKey, *asInt);
        return;
    }
    if (const std::optional<double> asDouble = toDoubleIfNumeric(source.value(sourceKey))) {
        target->insert(finalKey, *asDouble);
    }
}

QVariantMap extractUsageTelemetry(const QJsonObject &root)
{
    QVariantMap telemetry;
    telemetry.insert(QStringLiteral("raw"), root.toVariantMap());

    const QString model = root.value(QStringLiteral("model")).toString().trimmed();
    if (!model.isEmpty()) {
        telemetry.insert(QStringLiteral("model"), model);
    }
    const QString object = root.value(QStringLiteral("object")).toString().trimmed();
    if (!object.isEmpty()) {
        telemetry.insert(QStringLiteral("response_object"), object);
    }

    const QJsonValue usageValue = root.value(QStringLiteral("usage"));
    if (usageValue.isObject()) {
        const QJsonObject usageObj = usageValue.toObject();
        const QVariantMap usageVariant = usageObj.toVariantMap();
        if (!usageVariant.isEmpty()) {
            telemetry.insert(QStringLiteral("usage_raw"), usageVariant);
        }

        copyNumericField(usageObj, QStringLiteral("prompt_tokens"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("completion_tokens"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("total_tokens"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("input_tokens"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("output_tokens"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("reasoning_tokens"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("native_tokens_reasoning"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("native_tokens_prompt"), &telemetry);
        copyNumericField(usageObj, QStringLiteral("native_tokens_completion"), &telemetry);

        copyNumericField(usageObj, QStringLiteral("usage"), &telemetry, QStringLiteral("usage_usd"));
        copyNumericField(usageObj, QStringLiteral("total_cost"), &telemetry, QStringLiteral("total_cost_usd"));
        copyNumericField(usageObj, QStringLiteral("upstream_inference_cost"), &telemetry, QStringLiteral("upstream_inference_cost_usd"));
        copyNumericField(usageObj, QStringLiteral("byok_usage_inference"), &telemetry, QStringLiteral("byok_usage_inference_usd"));

        const QJsonObject inputDetails = usageObj.value(QStringLiteral("input_tokens_details")).toObject();
        if (!inputDetails.isEmpty()) {
            copyNumericField(inputDetails, QStringLiteral("cached_tokens"), &telemetry);
        }
        const QJsonObject outputDetails = usageObj.value(QStringLiteral("output_tokens_details")).toObject();
        if (!outputDetails.isEmpty()) {
            copyNumericField(outputDetails, QStringLiteral("reasoning_tokens"), &telemetry, QStringLiteral("output_reasoning_tokens"));
        }
    } else if (const std::optional<double> usageDouble = toDoubleIfNumeric(usageValue)) {
        telemetry.insert(QStringLiteral("usage_usd"), *usageDouble);
    }

    copyNumericField(root, QStringLiteral("prompt_tokens"), &telemetry);
    copyNumericField(root, QStringLiteral("completion_tokens"), &telemetry);
    copyNumericField(root, QStringLiteral("total_tokens"), &telemetry);
    copyNumericField(root, QStringLiteral("input_tokens"), &telemetry);
    copyNumericField(root, QStringLiteral("output_tokens"), &telemetry);
    copyNumericField(root, QStringLiteral("reasoning_tokens"), &telemetry);
    copyNumericField(root, QStringLiteral("native_tokens_reasoning"), &telemetry);
    copyNumericField(root, QStringLiteral("usage"), &telemetry, QStringLiteral("usage_usd"));
    copyNumericField(root, QStringLiteral("total_cost"), &telemetry, QStringLiteral("total_cost_usd"));
    copyNumericField(root, QStringLiteral("upstream_inference_cost"), &telemetry, QStringLiteral("upstream_inference_cost_usd"));
    copyNumericField(root, QStringLiteral("byok_usage_inference"), &telemetry, QStringLiteral("byok_usage_inference_usd"));

    if (!telemetry.contains(QStringLiteral("credits_spent_usd")) && telemetry.contains(QStringLiteral("usage_usd"))) {
        telemetry.insert(QStringLiteral("credits_spent_usd"), telemetry.value(QStringLiteral("usage_usd")));
    }

    telemetry.remove(QStringLiteral("raw"));
    telemetry.remove(QStringLiteral("usage_raw"));
    return telemetry;
}

bool hasUsageSignalFields(const QVariantMap &telemetry)
{
    static const QStringList keys{
        QStringLiteral("prompt_tokens"),
        QStringLiteral("completion_tokens"),
        QStringLiteral("total_tokens"),
        QStringLiteral("input_tokens"),
        QStringLiteral("output_tokens"),
        QStringLiteral("reasoning_tokens"),
        QStringLiteral("usage_usd"),
        QStringLiteral("total_cost_usd"),
        QStringLiteral("credits_spent_usd")
    };
    for (const QString &key : keys) {
        if (telemetry.contains(key)) {
            return true;
        }
    }
    return false;
}
}

OpenAiCompatibleClient::OpenAiCompatibleClient(QObject *parent)
    : AiBackendClient(parent)
{
    m_networkAccessManager = new QNetworkAccessManager(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        const quint64 expiredRequestId = m_activeRequestId;
        if (expiredRequestId == 0) {
            return;
        }

        QPointer<QNetworkReply> expiredReply = m_activeReply;
        m_activeRequestId = 0;
        m_activeReply = nullptr;
        m_streamBuffer.clear();
        m_streamedContent.clear();
        if (expiredReply) {
            expiredReply->abort();
        }
        finishWithFailure(expiredRequestId, QStringLiteral("%1 request timed out").arg(providerDisplayName()));
    });
}

void OpenAiCompatibleClient::setEndpoint(const QString &endpoint)
{
    m_endpoint = normalizeEndpoint(endpoint, m_providerKind);
    qInfo().noquote() << "[AI_PROVIDER] endpoint_set provider=" << m_providerKind
                      << " endpoint=" << m_endpoint;
}

void OpenAiCompatibleClient::setProviderConfig(const QString &providerKind, const QString &apiKey)
{
    m_providerKind = normalizedProviderKind(providerKind);
    m_apiKey = apiKey.trimmed();
    m_endpoint = normalizeEndpoint(m_endpoint, m_providerKind);
    qInfo().noquote() << "[AI_PROVIDER] config_set provider=" << m_providerKind
                      << " endpoint=" << m_endpoint
                      << " apiKeyConfigured=" << (!m_apiKey.isEmpty() ? "true" : "false");
}

QString OpenAiCompatibleClient::endpoint() const
{
    return m_endpoint;
}

void OpenAiCompatibleClient::fetchModels()
{
    qInfo().noquote() << "[AI_PROVIDER] fetch_models provider=" << m_providerKind
                      << " endpoint=" << m_endpoint;
    auto *reply = m_networkAccessManager->get(buildJsonRequest(QStringLiteral("/v1/models")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto cleanup = qScopeGuard([reply]() { reply->deleteLater(); });
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning().noquote() << "[AI_PROVIDER] fetch_models_failed provider=" << m_providerKind
                                 << " endpoint=" << m_endpoint
                                 << " httpStatus=" << httpStatus
                                 << " error=" << reply->errorString();
            emit availabilityChanged({false, false, QStringLiteral("%1 offline").arg(providerDisplayName())});
            emit modelsReady({});
            return;
        }

        const auto document = QJsonDocument::fromJson(reply->readAll());
        QList<ModelInfo> models;
        for (const auto &value : document.object().value(QStringLiteral("data")).toArray()) {
            const auto object = value.toObject();
            models.push_back({
                .id = object.value(QStringLiteral("id")).toString(),
                .ownedBy = object.value(QStringLiteral("owned_by")).toString()
            });
        }

        qInfo().noquote() << "[AI_PROVIDER] fetch_models_ready provider=" << m_providerKind
                          << " endpoint=" << m_endpoint
                          << " httpStatus=" << httpStatus
                          << " count=" << models.size();
        emit availabilityChanged({true, !models.isEmpty(), QStringLiteral("%1 connected").arg(providerDisplayName())});
        emit modelsReady(models);
        probeCapabilities();
    });
}

AgentCapabilitySet OpenAiCompatibleClient::capabilities() const
{
    return m_capabilities;
}

quint64 OpenAiCompatibleClient::sendChatRequest(const QList<AiMessage> &messages, const QString &model, const AiRequestOptions &options)
{
    m_timeoutTimer->stop();
    m_activeRequestId = 0;
    m_activeReply = nullptr;
    m_streamBuffer.clear();
    m_streamedContent.clear();

    QJsonArray jsonMessages;
    for (const auto &message : messages) {
        jsonMessages.push_back(QJsonObject{
            {QStringLiteral("role"), message.role},
            {QStringLiteral("content"), message.content}
        });
    }

    QJsonObject body{
        {QStringLiteral("model"), model},
        {QStringLiteral("messages"), jsonMessages},
        {QStringLiteral("temperature"), options.temperature},
        {QStringLiteral("stream"), options.stream}
    };

    if (options.topP.has_value()) {
        body.insert(QStringLiteral("top_p"), options.topP.value());
    }
    if (options.providerTopK.has_value()) {
        body.insert(QStringLiteral("top_k"), options.providerTopK.value());
    }
    if (options.maxTokens.has_value() && options.maxTokens.value() > 0) {
        body.insert(QStringLiteral("max_tokens"), options.maxTokens.value());
    }
    if (options.seed.has_value()) {
        body.insert(QStringLiteral("seed"), options.seed.value());
    }

    m_activeRequestId = ++m_requestCounter;
    qInfo().noquote() << "[AI_PROVIDER] chat_request_start requestId=" << m_activeRequestId
                      << " provider=" << m_providerKind
                      << " endpoint=" << m_endpoint
                      << " model=" << model
                      << " stream=" << (options.stream ? "true" : "false")
                      << " messageCount=" << messages.size();

    QNetworkRequest request = buildJsonRequest(QStringLiteral("/v1/chat/completions"));
    m_activeReply = m_networkAccessManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    const quint64 requestId = m_activeRequestId;
    QPointer<QNetworkReply> reply = m_activeReply;
    emit requestStarted(requestId);
    m_timeoutTimer->start(static_cast<int>(options.timeout.count()));

    if (options.stream) {
        connect(reply, &QIODevice::readyRead, this, [this, requestId, reply]() {
            if (!reply || reply != m_activeReply || requestId != m_activeRequestId) {
                return;
            }
            handleStreamingReply(requestId, reply);
        });
    }

    connect(reply, &QNetworkReply::finished, this, [this, options, requestId, reply]() {
        if (!reply) {
            return;
        }

        const bool staleReply = reply != m_activeReply || requestId != m_activeRequestId;
        if (staleReply) {
            reply->deleteLater();
            return;
        }

        m_timeoutTimer->stop();
        const auto cleanup = qScopeGuard([this, reply]() {
            reply->deleteLater();
            m_activeReply = nullptr;
        });

        if (reply->error() != QNetworkReply::NoError) {
            finishWithFailure(m_activeRequestId, parseErrorMessage(reply));
            return;
        }

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (options.stream) {
            handleStreamingReply(requestId, reply);
            qInfo().noquote() << "[AI_PROVIDER] chat_request_finished requestId=" << requestId
                              << " provider=" << m_providerKind
                              << " endpoint=" << m_endpoint
                              << " httpStatus=" << httpStatus
                              << " chars=" << m_streamedContent.size();
            emit requestFinished(requestId, m_streamedContent);
            return;
        }

        const QByteArray payload = reply->readAll();
        const auto document = QJsonDocument::fromJson(payload);
        const QVariantMap usageTelemetry = document.isObject()
            ? extractUsageTelemetry(document.object())
            : QVariantMap{};
        if (hasUsageSignalFields(usageTelemetry)) {
            QVariantMap usage = usageTelemetry;
            usage.insert(QStringLiteral("provider"), normalizedProviderKind(m_providerKind));
            usage.insert(QStringLiteral("request_kind"), QStringLiteral("conversation"));
            usage.insert(QStringLiteral("reasoning_effort"), reasoningEffortForMode(options.mode));
            emit requestUsageUpdated(requestId, usage);
        }
        const auto choices = document.object().value(QStringLiteral("choices")).toArray();
        const auto content = choices.isEmpty()
            ? QString{}
            : choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
        qInfo().noquote() << "[AI_PROVIDER] chat_request_finished requestId=" << requestId
                          << " provider=" << m_providerKind
                          << " endpoint=" << m_endpoint
                          << " httpStatus=" << httpStatus
                          << " chars=" << content.size();
        emit requestFinished(requestId, content);
    });

    return m_activeRequestId;
}

quint64 OpenAiCompatibleClient::sendAgentRequest(const AgentRequest &request)
{
    m_timeoutTimer->stop();
    m_activeRequestId = 0;
    m_activeReply = nullptr;
    m_capabilities.selectedModelToolCapable = modelLooksToolCapable(request.model)
        || (m_capabilities.responsesApi && m_capabilities.toolCalling);
    m_capabilities.agentEnabled = m_capabilities.responsesApi && m_capabilities.selectedModelToolCapable;
    emit capabilitiesChanged(m_capabilities);

    QJsonObject body{
        {QStringLiteral("model"), request.model},
        {QStringLiteral("stream"), false}
    };
    if (!request.instructions.trimmed().isEmpty()) {
        body.insert(QStringLiteral("instructions"), request.instructions);
    }
    if (!request.previousResponseId.trimmed().isEmpty()) {
        body.insert(QStringLiteral("previous_response_id"), request.previousResponseId);
    }

    if (!request.toolResults.isEmpty()) {
        QJsonArray inputItems;
        for (const auto &result : request.toolResults) {
            inputItems.push_back(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("function_call_output")},
                {QStringLiteral("call_id"), result.callId},
                {QStringLiteral("output"), result.output}
            });
        }
        body.insert(QStringLiteral("input"), inputItems);
    } else {
        body.insert(QStringLiteral("input"), request.inputText);
    }

    if (!request.tools.isEmpty()) {
        QJsonArray tools;
        for (const auto &tool : request.tools) {
            tools.push_back(functionToolSchema(tool));
        }
        body.insert(QStringLiteral("tools"), tools);
    }

    body.insert(QStringLiteral("temperature"), request.toolResults.isEmpty()
        ? request.sampling.conversationTemperature
        : request.sampling.toolUseTemperature);
    if (request.sampling.conversationTopP.has_value()) {
        body.insert(QStringLiteral("top_p"), *request.sampling.conversationTopP);
    }
    if (request.sampling.providerTopK.has_value()) {
        body.insert(QStringLiteral("top_k"), *request.sampling.providerTopK);
    }
    if (request.sampling.maxOutputTokens > 0) {
        body.insert(QStringLiteral("max_output_tokens"), request.sampling.maxOutputTokens);
    }
    if (shouldIncludeReasoningObject(m_providerKind)) {
        const QString effort = reasoningEffortForMode(request.mode);
        body.insert(QStringLiteral("reasoning"), QJsonObject{{QStringLiteral("effort"), effort}});
    }

    m_activeRequestId = ++m_requestCounter;
    qInfo().noquote() << "[AI_PROVIDER] agent_request_start requestId=" << m_activeRequestId
                      << " provider=" << m_providerKind
                      << " endpoint=" << m_endpoint
                      << " model=" << request.model
                      << " hasPreviousResponse=" << (!request.previousResponseId.trimmed().isEmpty() ? "true" : "false")
                      << " toolResultCount=" << request.toolResults.size();
    QNetworkRequest httpRequest = buildJsonRequest(QStringLiteral("/v1/responses"));
    m_activeReply = m_networkAccessManager->post(httpRequest, QJsonDocument(body).toJson(QJsonDocument::Compact));
    const quint64 requestId = m_activeRequestId;
    QPointer<QNetworkReply> reply = m_activeReply;
    emit requestStarted(requestId);
    m_timeoutTimer->start(static_cast<int>(request.timeout.count()));

    connect(reply, &QNetworkReply::finished, this, [this, request, requestId, reply]() {
        if (!reply) {
            return;
        }

        const bool staleReply = reply != m_activeReply || requestId != m_activeRequestId;
        if (staleReply) {
            reply->deleteLater();
            return;
        }

        m_timeoutTimer->stop();
        const auto cleanup = qScopeGuard([this, reply]() {
            reply->deleteLater();
            m_activeReply = nullptr;
        });

        if (reply->error() != QNetworkReply::NoError) {
            finishWithFailure(m_activeRequestId, parseErrorMessage(reply));
            return;
        }

        const QByteArray payload = reply->readAll();
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        if (document.isObject()) {
            const QVariantMap usageTelemetry = extractUsageTelemetry(document.object());
            if (hasUsageSignalFields(usageTelemetry)) {
                QVariantMap usage = usageTelemetry;
                usage.insert(QStringLiteral("provider"), normalizedProviderKind(m_providerKind));
                usage.insert(QStringLiteral("request_kind"), QStringLiteral("agent"));
                usage.insert(QStringLiteral("reasoning_effort"), reasoningEffortForMode(request.mode));
                emit requestUsageUpdated(requestId, usage);
            }
        }
        const AgentResponse response = parseAgentResponse(payload);
        if (response.toolCalls.isEmpty() && response.outputText.trimmed().isEmpty()) {
            finishWithFailure(m_activeRequestId, QStringLiteral("Provider returned an empty agent response payload"));
            return;
        }
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qInfo().noquote() << "[AI_PROVIDER] agent_request_finished requestId=" << requestId
                          << " provider=" << m_providerKind
                          << " endpoint=" << m_endpoint
                          << " httpStatus=" << httpStatus
                          << " responseChars=" << response.outputText.size()
                          << " toolCalls=" << response.toolCalls.size();
        emit agentResponseReady(requestId, response);
    });

    return m_activeRequestId;
}

void OpenAiCompatibleClient::cancelActiveRequest()
{
    QPointer<QNetworkReply> reply = m_activeReply;
    m_timeoutTimer->stop();
    m_activeRequestId = 0;
    m_activeReply = nullptr;
    m_streamBuffer.clear();
    m_streamedContent.clear();
    if (reply) {
        reply->abort();
    }
}

void OpenAiCompatibleClient::finishWithFailure(quint64 requestId, const QString &errorText)
{
    qWarning().noquote() << "[AI_PROVIDER] request_failed requestId=" << requestId
                         << " provider=" << m_providerKind
                         << " endpoint=" << m_endpoint
                         << " error=" << errorText;
    emit requestFailed(requestId, errorText);
}

QNetworkRequest OpenAiCompatibleClient::buildJsonRequest(const QString &path) const
{
    QNetworkRequest request(QUrl(m_endpoint + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    }
    if (m_providerKind == QStringLiteral("openrouter")) {
        request.setRawHeader("HTTP-Referer", "https://www.vaxil.app/");
        request.setRawHeader("X-Title", "Vaxil");
    }
    qInfo().noquote() << "[AI_PROVIDER] build_request provider=" << m_providerKind
                      << " endpoint=" << m_endpoint
                      << " path=" << path
                      << " hasAuthHeader=" << (!m_apiKey.isEmpty() ? "true" : "false")
                      << " openRouterHeaders=" << (m_providerKind == QStringLiteral("openrouter") ? "true" : "false");
    return request;
}

QString OpenAiCompatibleClient::providerDisplayName() const
{
    if (m_providerKind == QStringLiteral("openrouter")) {
        return QStringLiteral("OpenRouter");
    }
    if (m_providerKind == QStringLiteral("ollama")) {
        return QStringLiteral("Ollama");
    }
    return QStringLiteral("Local AI backend");
}

void OpenAiCompatibleClient::probeCapabilities()
{
    qInfo().noquote() << "[AI_PROVIDER] probe_capabilities provider=" << m_providerKind
                      << " endpoint=" << m_endpoint;
    auto *reply = m_networkAccessManager->sendCustomRequest(buildJsonRequest(QStringLiteral("/v1/responses")), "OPTIONS");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto cleanup = qScopeGuard([reply]() { reply->deleteLater(); });
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool responsesApi = httpStatus != 404 && reply->error() != QNetworkReply::HostNotFoundError;
        m_capabilities.responsesApi = responsesApi;
        m_capabilities.previousResponseId = responsesApi;
        m_capabilities.toolCalling = responsesApi;
        m_capabilities.remoteMcp = false;
        m_capabilities.selectedModelToolCapable = true;
        m_capabilities.agentEnabled = responsesApi;
        m_capabilities.providerMode = responsesApi ? QStringLiteral("responses") : QStringLiteral("chat_completions");
        m_capabilities.status = responsesApi
            ? QStringLiteral("Agent tools available")
            : QStringLiteral("Agent tools unavailable; using chat completions fallback");
        qInfo().noquote() << "[AI_PROVIDER] capabilities_ready provider=" << m_providerKind
                          << " endpoint=" << m_endpoint
                          << " httpStatus=" << httpStatus
                          << " responsesApi=" << (responsesApi ? "true" : "false")
                          << " providerMode=" << m_capabilities.providerMode;
        emit capabilitiesChanged(m_capabilities);
    });
}

AgentResponse OpenAiCompatibleClient::parseAgentResponse(const QByteArray &payload) const
{
    AgentResponse response;
    response.rawJson = QString::fromUtf8(payload);
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return response;
    }

    const QJsonObject object = document.object();
    response.responseId = object.value(QStringLiteral("id")).toString();
    const QJsonArray output = object.value(QStringLiteral("output")).toArray();
    QStringList textParts;

    for (const QJsonValue &itemValue : output) {
        const QJsonObject item = itemValue.toObject();
        const QString type = item.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("message")) {
            const QJsonArray content = item.value(QStringLiteral("content")).toArray();
            for (const QJsonValue &contentValue : content) {
                const QJsonObject contentObject = contentValue.toObject();
                const QString contentType = contentObject.value(QStringLiteral("type")).toString();
                if (contentType == QStringLiteral("output_text")
                    || contentType == QStringLiteral("text")) {
                    const QString text = firstNonEmptyTextField(
                        contentObject,
                        {QStringLiteral("text"), QStringLiteral("content"), QStringLiteral("value")});
                    if (!text.trimmed().isEmpty()) {
                        textParts.push_back(text);
                    }
                }
            }
        } else if (type == QStringLiteral("function_call")) {
            response.toolCalls.push_back({
                .id = item.value(QStringLiteral("call_id")).toString(),
                .name = item.value(QStringLiteral("name")).toString(),
                .argumentsJson = item.value(QStringLiteral("arguments")).toString()
            });
        } else if (type == QStringLiteral("output_text") || type == QStringLiteral("text")) {
            const QString text = firstNonEmptyTextField(
                item,
                {QStringLiteral("text"), QStringLiteral("content"), QStringLiteral("value")});
            if (!text.trimmed().isEmpty()) {
                textParts.push_back(text);
            }
        }
    }

    if (textParts.isEmpty()) {
        response.outputText = firstNonEmptyTextField(
            object,
            {
                QStringLiteral("output_text"),
                QStringLiteral("text"),
                QStringLiteral("content"),
                QStringLiteral("message"),
                QStringLiteral("output")
            });
    } else {
        response.outputText = textParts.join(QString());
    }

    if (containsPromptLeakMarkers(response.outputText)) {
        response.outputText.clear();
    }
    return response;
}

QString OpenAiCompatibleClient::reasoningEffortForMode(ReasoningMode mode)
{
    switch (mode) {
    case ReasoningMode::Fast:
        return QStringLiteral("low");
    case ReasoningMode::Deep:
        return QStringLiteral("high");
    case ReasoningMode::Balanced:
    default:
        return QStringLiteral("medium");
    }
}

bool OpenAiCompatibleClient::modelLooksToolCapable(const QString &modelId)
{
    const QString lowered = modelId.toLower();
    return lowered.contains(QStringLiteral("qwen"))
        || lowered.contains(QStringLiteral("granite"))
        || lowered.contains(QStringLiteral("llama"))
        || lowered.contains(QStringLiteral("gpt"))
        || lowered.contains(QStringLiteral("claude"))
        || lowered.contains(QStringLiteral("gemini"))
        || lowered.contains(QStringLiteral("mistral"))
        || lowered.contains(QStringLiteral("deepseek"))
        || lowered.contains(QStringLiteral("gpt-oss"))
        || lowered.contains(QStringLiteral("tool"));
}

QString OpenAiCompatibleClient::parseErrorMessage(QNetworkReply *reply) const
{
    const QByteArray body = reply != nullptr ? reply->readAll() : QByteArray{};
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QJsonObject root = document.object();
        if (root.contains(QStringLiteral("error"))) {
            const QJsonValue error = root.value(QStringLiteral("error"));
            if (error.isObject()) {
                const QString message = error.toObject().value(QStringLiteral("message")).toString();
                if (!message.isEmpty()) {
                    return message;
                }
            } else if (error.isString()) {
                return error.toString();
            }
        }

        const QString message = root.value(QStringLiteral("message")).toString();
        if (!message.isEmpty()) {
            return message;
        }
    }

    if (!body.trimmed().isEmpty()) {
        return QString::fromUtf8(body).trimmed();
    }

    return reply != nullptr ? reply->errorString() : QStringLiteral("%1 request failed").arg(providerDisplayName());
}

void OpenAiCompatibleClient::handleStreamingReply(quint64 requestId, QNetworkReply *reply)
{
    m_streamBuffer += reply->readAll();

    if (reply != nullptr && reply->isFinished() && !m_streamBuffer.endsWith("\n\n") && !m_streamBuffer.endsWith("\r\n\r\n")) {
        m_streamBuffer.append("\n\n");
    }

    int separatorSize = 0;
    int separatorIndex = m_streamBuffer.indexOf("\r\n\r\n");
    if (separatorIndex >= 0) {
        separatorSize = 4;
    } else {
        separatorIndex = m_streamBuffer.indexOf("\n\n");
        if (separatorIndex >= 0) {
            separatorSize = 2;
        }
    }

    while (separatorIndex >= 0) {
        QByteArray rawEvent = m_streamBuffer.left(separatorIndex);
        m_streamBuffer.remove(0, separatorIndex + separatorSize);

        rawEvent.replace("\r", "");
        const QList<QByteArray> lines = rawEvent.split('\n');

        QByteArray payload;
        for (const QByteArray &lineRaw : lines) {
            const QByteArray line = lineRaw.trimmed();
            if (line.startsWith("data:")) {
                QByteArray segment = line.mid(5);
                if (segment.startsWith(' ')) {
                    segment.remove(0, 1);
                }
                if (!payload.isEmpty()) {
                    payload.append('\n');
                }
                payload.append(segment);
            }
        }

        if (payload.isEmpty() || payload == "[DONE]") {
            separatorIndex = m_streamBuffer.indexOf("\n\n");
            continue;
        }

        const QJsonDocument document = QJsonDocument::fromJson(payload);
        if (!document.isObject()) {
            separatorIndex = m_streamBuffer.indexOf("\n\n");
            continue;
        }

        const QJsonObject object = document.object();
        const QVariantMap usageTelemetry = extractUsageTelemetry(object);
        if (hasUsageSignalFields(usageTelemetry)) {
            QVariantMap usage = usageTelemetry;
            usage.insert(QStringLiteral("provider"), normalizedProviderKind(m_providerKind));
            usage.insert(QStringLiteral("request_kind"), QStringLiteral("conversation"));
            emit requestUsageUpdated(requestId, usage);
        }

        const QString eventType = object.value(QStringLiteral("type")).toString();
        if (eventType == QStringLiteral("response.output_text.delta")) {
            const QString delta = object.value(QStringLiteral("delta")).toString();
            if (!delta.isEmpty()) {
                m_streamedContent += delta;
                emit requestDelta(requestId, delta);
            }
            separatorIndex = m_streamBuffer.indexOf("\n\n");
            continue;
        }

        const QJsonArray choices = object.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
            const QJsonObject choice = choices.first().toObject();
            const QJsonObject deltaObject = choice.value(QStringLiteral("delta")).toObject();
            const QString delta = deltaObject.value(QStringLiteral("content")).toString();
            if (!delta.isEmpty()) {
                m_streamedContent += delta;
                emit requestDelta(requestId, delta);
            }
        }

        separatorIndex = m_streamBuffer.indexOf("\r\n\r\n");
        if (separatorIndex >= 0) {
            separatorSize = 4;
        } else {
            separatorIndex = m_streamBuffer.indexOf("\n\n");
            separatorSize = separatorIndex >= 0 ? 2 : 0;
        }
    }
}
