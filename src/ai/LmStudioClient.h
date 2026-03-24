#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "core/AssistantTypes.h"

class LmStudioClient : public QObject
{
    Q_OBJECT

public:
    explicit LmStudioClient(QObject *parent = nullptr);

    void setEndpoint(const QString &endpoint);
    QString endpoint() const;

    void fetchModels();
    quint64 sendChatRequest(const QList<AiMessage> &messages, const QString &model, const AiRequestOptions &options);
    void cancelActiveRequest();

signals:
    void modelsReady(const QList<ModelInfo> &models);
    void availabilityChanged(const AiAvailability &availability);
    void requestStarted(quint64 requestId);
    void requestDelta(quint64 requestId, const QString &delta);
    void requestFinished(quint64 requestId, const QString &fullText);
    void requestFailed(quint64 requestId, const QString &errorText);

private:
    void finishWithFailure(quint64 requestId, const QString &errorText);
    QNetworkRequest buildJsonRequest(const QString &path) const;
    QString parseErrorMessage(QNetworkReply *reply) const;
    void handleStreamingReply(quint64 requestId, QNetworkReply *reply);

    QNetworkAccessManager m_networkAccessManager;
    QString m_endpoint = QStringLiteral("http://localhost:1234");
    QPointer<QNetworkReply> m_activeReply;
    QTimer m_timeoutTimer;
    quint64 m_requestCounter = 0;
    quint64 m_activeRequestId = 0;
    QByteArray m_streamBuffer;
    QString m_streamedContent;
};
