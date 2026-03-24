#include "ai/LmStudioClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QScopeGuard>
#include <QUrl>

LmStudioClient::LmStudioClient(QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_activeReply) {
            m_activeReply->abort();
        }
        finishWithFailure(m_activeRequestId, QStringLiteral("LM Studio request timed out"));
    });
}

void LmStudioClient::setEndpoint(const QString &endpoint)
{
    m_endpoint = endpoint;
}

QString LmStudioClient::endpoint() const
{
    return m_endpoint;
}

void LmStudioClient::fetchModels()
{
    auto *reply = m_networkAccessManager.get(buildJsonRequest(QStringLiteral("/v1/models")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto cleanup = qScopeGuard([reply]() { reply->deleteLater(); });

        if (reply->error() != QNetworkReply::NoError) {
            emit availabilityChanged({false, false, QStringLiteral("LM Studio offline")});
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

        emit availabilityChanged({true, !models.isEmpty(), QStringLiteral("LM Studio connected")});
        emit modelsReady(models);
    });
}

quint64 LmStudioClient::sendChatRequest(const QList<AiMessage> &messages, const QString &model, const AiRequestOptions &options)
{
    cancelActiveRequest();

    QJsonArray jsonMessages;
    for (const auto &message : messages) {
        jsonMessages.push_back(QJsonObject{
            {QStringLiteral("role"), message.role},
            {QStringLiteral("content"), message.content}
        });
    }

    const QJsonObject body{
        {QStringLiteral("model"), model},
        {QStringLiteral("messages"), jsonMessages},
        {QStringLiteral("temperature"), 0.7},
        {QStringLiteral("stream"), options.stream}
    };

    m_activeRequestId = ++m_requestCounter;
    m_streamBuffer.clear();
    m_streamedContent.clear();

    QNetworkRequest request = buildJsonRequest(QStringLiteral("/v1/chat/completions"));
    m_activeReply = m_networkAccessManager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    emit requestStarted(m_activeRequestId);
    m_timeoutTimer.start(static_cast<int>(options.timeout.count()));

    if (options.stream) {
        connect(m_activeReply, &QIODevice::readyRead, this, [this]() {
            handleStreamingReply(m_activeRequestId, m_activeReply);
        });
    }

    connect(m_activeReply, &QNetworkReply::finished, this, [this, options]() {
        if (!m_activeReply) {
            return;
        }

        const auto reply = m_activeReply;
        m_timeoutTimer.stop();
        const auto cleanup = qScopeGuard([this, reply]() {
            reply->deleteLater();
            m_activeReply = nullptr;
        });

        if (reply->error() != QNetworkReply::NoError) {
            finishWithFailure(m_activeRequestId, reply->errorString());
            return;
        }

        if (options.stream) {
            handleStreamingReply(m_activeRequestId, reply);
            emit requestFinished(m_activeRequestId, m_streamedContent);
            return;
        }

        const auto document = QJsonDocument::fromJson(reply->readAll());
        const auto choices = document.object().value(QStringLiteral("choices")).toArray();
        const auto content = choices.isEmpty()
            ? QString{}
            : choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
        emit requestFinished(m_activeRequestId, content);
    });

    return m_activeRequestId;
}

void LmStudioClient::cancelActiveRequest()
{
    m_timeoutTimer.stop();
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    m_activeRequestId = 0;
}

void LmStudioClient::finishWithFailure(quint64 requestId, const QString &errorText)
{
    emit requestFailed(requestId, errorText);
}

QNetworkRequest LmStudioClient::buildJsonRequest(const QString &path) const
{
    QNetworkRequest request(QUrl(m_endpoint + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return request;
}

void LmStudioClient::handleStreamingReply(quint64 requestId, QNetworkReply *reply)
{
    m_streamBuffer += reply->readAll();
    const QList<QByteArray> events = m_streamBuffer.split('\n');
    const bool hasPartialTail = !m_streamBuffer.endsWith('\n');
    if (hasPartialTail && !events.isEmpty()) {
        m_streamBuffer = events.last();
    } else {
        m_streamBuffer.clear();
    }

    const int completeCount = hasPartialTail ? events.size() - 1 : events.size();
    for (int i = 0; i < completeCount; ++i) {
        const QByteArray line = events.at(i).trimmed();
        if (!line.startsWith("data: ")) {
            continue;
        }

        const QByteArray payload = line.mid(6);
        if (payload == "[DONE]") {
            continue;
        }

        const auto document = QJsonDocument::fromJson(payload);
        const auto choices = document.object().value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            continue;
        }

        const auto delta = choices.first().toObject().value(QStringLiteral("delta")).toObject().value(QStringLiteral("content")).toString();
        if (delta.isEmpty()) {
            continue;
        }

        m_streamedContent += delta;
        emit requestDelta(requestId, delta);
    }
}
