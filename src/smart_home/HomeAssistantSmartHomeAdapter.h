#pragma once

#include <functional>

#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include "smart_home/SmartHomePorts.h"

class QNetworkAccessManager;
class QNetworkReply;

class HomeAssistantSmartHomeAdapter final : public QObject,
                                            public SmartHomeStatePort,
                                            public SmartLightControlPort
{
public:
    explicit HomeAssistantSmartHomeAdapter(const SmartHomeConfig &config, QObject *parent = nullptr);

    void setConfig(const SmartHomeConfig &config);
    SmartHomeConfig config() const;

    void fetchState(std::function<void(const SmartHomeSnapshot &)> callback) override;
    SmartHomeSnapshot fetchStateBlocking() override;

    void executeLightCommand(const SmartLightCommand &command,
                             std::function<void(const SmartLightCommandResult &)> callback) override;
    SmartLightCommandResult executeLightCommandBlocking(const SmartLightCommand &command) override;

    static SmartPresenceSnapshot parsePresenceState(const QJsonObject &stateObject,
                                                    const QString &entityId,
                                                    qint64 observedAtMs);
    static SmartLightSnapshot parseLightState(const QJsonObject &stateObject,
                                              const QString &entityId,
                                              qint64 observedAtMs);

private:
    QUrl stateUrl(const QString &entityId) const;
    QUrl serviceUrl(const QString &domain, const QString &service) const;
    QNetworkRequest buildRequest(const QUrl &url, bool jsonBody = false) const;
    QString bearerToken() const;
    bool hasUsableConfig(QString *detail = nullptr) const;

    SmartHomeConfig m_config;
    QNetworkAccessManager *m_network = nullptr;
};
