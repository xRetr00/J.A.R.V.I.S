#include "smart_home/HomeAssistantSmartHomeAdapter.h"

#include <algorithm>

#include <QEventLoop>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>

namespace {
QString trimmedBaseUrl(QString value)
{
    value = value.trimmed();
    while (value.endsWith(QLatin1Char('/'))) {
        value.chop(1);
    }
    return value;
}

bool isUnavailableState(const QString &state)
{
    const QString lowered = state.trimmed().toLower();
    return lowered.isEmpty()
        || lowered == QStringLiteral("unknown")
        || lowered == QStringLiteral("unavailable")
        || lowered == QStringLiteral("none");
}

bool isTruthyPresenceState(const QString &state)
{
    const QString lowered = state.trimmed().toLower();
    if (lowered == QStringLiteral("on")
        || lowered == QStringLiteral("occupied")
        || lowered == QStringLiteral("detected")
        || lowered == QStringLiteral("true")
        || lowered == QStringLiteral("home")) {
        return true;
    }
    bool ok = false;
    const double numeric = lowered.toDouble(&ok);
    return ok && numeric > 0.0;
}

int brightnessPercentFromAttributes(const QJsonObject &attributes)
{
    if (!attributes.contains(QStringLiteral("brightness"))) {
        return -1;
    }
    const int raw = attributes.value(QStringLiteral("brightness")).toInt(-1);
    if (raw < 0) {
        return -1;
    }
    return std::clamp(static_cast<int>((raw / 255.0) * 100.0 + 0.5), 0, 100);
}

bool hasColorSupport(const QJsonObject &attributes)
{
    if (attributes.contains(QStringLiteral("rgb_color"))
        || attributes.contains(QStringLiteral("hs_color"))
        || attributes.contains(QStringLiteral("color_mode"))) {
        return true;
    }
    const QJsonArray modes = attributes.value(QStringLiteral("supported_color_modes")).toArray();
    for (const QJsonValue &mode : modes) {
        const QString value = mode.toString().toLower();
        if (value.contains(QStringLiteral("rgb"))
            || value.contains(QStringLiteral("hs"))
            || value.contains(QStringLiteral("color_temp"))
            || value.contains(QStringLiteral("xy"))) {
            return true;
        }
    }
    return false;
}

QString colorNameFromAttributes(const QJsonObject &attributes)
{
    const QString colorMode = attributes.value(QStringLiteral("color_mode")).toString().trimmed();
    if (!colorMode.isEmpty()) {
        return colorMode;
    }
    if (attributes.contains(QStringLiteral("rgb_color"))) {
        const QJsonArray rgb = attributes.value(QStringLiteral("rgb_color")).toArray();
        if (rgb.size() >= 3) {
            return QStringLiteral("rgb(%1,%2,%3)")
                .arg(rgb.at(0).toInt())
                .arg(rgb.at(1).toInt())
                .arg(rgb.at(2).toInt());
        }
    }
    return {};
}

SmartHomeSnapshot unavailableSnapshot(const SmartHomeConfig &config, const QString &summary, const QString &detail, const QString &errorKind)
{
    SmartHomeSnapshot snapshot;
    snapshot.success = false;
    snapshot.summary = summary;
    snapshot.detail = detail;
    snapshot.errorKind = errorKind;
    snapshot.presence.sensorId = config.presenceEntityId;
    snapshot.light.lightId = config.lightEntityId;
    snapshot.presence.source = QStringLiteral("home_assistant");
    snapshot.light.source = QStringLiteral("home_assistant");
    snapshot.presence.stale = true;
    snapshot.light.stale = true;
    return snapshot;
}

struct BlockingHttpResult
{
    bool ok = false;
    int httpStatus = 0;
    qint64 latencyMs = 0;
    QByteArray body;
    QString error;
};

BlockingHttpResult blockingRequest(const QNetworkRequest &request,
                                   const QByteArray &body,
                                   bool post,
                                   int timeoutMs)
{
    BlockingHttpResult result;
    QNetworkAccessManager manager;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply *reply = post ? manager.post(request, body) : manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (reply->isRunning()) {
            reply->abort();
        }
        loop.quit();
    });

    const qint64 startedAtMs = QDateTime::currentMSecsSinceEpoch();
    timer.start(std::max(500, timeoutMs));
    loop.exec();
    result.latencyMs = std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - startedAtMs);
    result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    result.ok = reply->error() == QNetworkReply::NoError && result.httpStatus >= 200 && result.httpStatus < 300;
    if (!result.ok) {
        result.error = reply->errorString();
        if (result.error.trimmed().isEmpty()) {
            result.error = QStringLiteral("HTTP %1").arg(result.httpStatus);
        }
    }
    reply->deleteLater();
    return result;
}
}

HomeAssistantSmartHomeAdapter::HomeAssistantSmartHomeAdapter(const SmartHomeConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_network(new QNetworkAccessManager(this))
{
}

void HomeAssistantSmartHomeAdapter::setConfig(const SmartHomeConfig &config)
{
    m_config = config;
}

SmartHomeConfig HomeAssistantSmartHomeAdapter::config() const
{
    return m_config;
}

void HomeAssistantSmartHomeAdapter::fetchState(std::function<void(const SmartHomeSnapshot &)> callback)
{
    QString configError;
    if (!hasUsableConfig(&configError)) {
        callback(unavailableSnapshot(m_config, QStringLiteral("Smart room unavailable"), configError, QStringLiteral("config")));
        return;
    }

    auto aggregate = std::make_shared<SmartHomeSnapshot>();
    auto remaining = std::make_shared<int>(0);
    const qint64 startedAtMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 observedAtMs = startedAtMs;
    aggregate->success = true;

    const auto finishOne = [aggregate, remaining, callback, startedAtMs]() {
        --(*remaining);
        if (*remaining > 0) {
            return;
        }
        aggregate->latencyMs = std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - startedAtMs);
        aggregate->summary = aggregate->success ? QStringLiteral("Smart room state updated") : QStringLiteral("Smart room unavailable");
        callback(*aggregate);
    };

    const auto queueGet = [&](const QString &entityId, bool presence) {
        if (entityId.trimmed().isEmpty()) {
            return;
        }
        ++(*remaining);
        QNetworkReply *reply = m_network->get(buildRequest(stateUrl(entityId)));
        QTimer::singleShot(std::max(500, m_config.requestTimeoutMs), reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
        connect(reply, &QNetworkReply::finished, this, [=]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            aggregate->httpStatus = httpStatus;
            if (reply->error() != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
                aggregate->success = false;
                aggregate->detail = reply->errorString();
                aggregate->errorKind = httpStatus == 401 || httpStatus == 403 ? QStringLiteral("auth") : QStringLiteral("transport");
            } else {
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (doc.isObject()) {
                    if (presence) {
                        aggregate->presence = parsePresenceState(doc.object(), entityId, observedAtMs);
                    } else {
                        aggregate->light = parseLightState(doc.object(), entityId, observedAtMs);
                    }
                }
            }
            reply->deleteLater();
            finishOne();
        });
    };

    queueGet(m_config.presenceEntityId, true);
    queueGet(m_config.lightEntityId, false);
    if (*remaining == 0) {
        callback(unavailableSnapshot(m_config,
                                     QStringLiteral("Smart room unavailable"),
                                     QStringLiteral("No Home Assistant entity ids are configured."),
                                     QStringLiteral("config")));
    }
}

SmartHomeSnapshot HomeAssistantSmartHomeAdapter::fetchStateBlocking()
{
    QString configError;
    if (!hasUsableConfig(&configError)) {
        return unavailableSnapshot(m_config, QStringLiteral("Smart room unavailable"), configError, QStringLiteral("config"));
    }

    SmartHomeSnapshot snapshot;
    snapshot.success = true;
    const qint64 startedAtMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 observedAtMs = startedAtMs;

    auto fetchEntity = [&](const QString &entityId, bool presence) {
        if (entityId.trimmed().isEmpty()) {
            return;
        }
        const BlockingHttpResult result = blockingRequest(buildRequest(stateUrl(entityId)), {}, false, m_config.requestTimeoutMs);
        snapshot.httpStatus = result.httpStatus;
        if (!result.ok) {
            snapshot.success = false;
            snapshot.detail = result.error;
            snapshot.errorKind = result.httpStatus == 401 || result.httpStatus == 403 ? QStringLiteral("auth") : QStringLiteral("transport");
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(result.body);
        if (!doc.isObject()) {
            snapshot.success = false;
            snapshot.detail = QStringLiteral("Home Assistant returned invalid JSON.");
            snapshot.errorKind = QStringLiteral("invalid");
            return;
        }
        if (presence) {
            snapshot.presence = parsePresenceState(doc.object(), entityId, observedAtMs);
        } else {
            snapshot.light = parseLightState(doc.object(), entityId, observedAtMs);
        }
    };

    fetchEntity(m_config.presenceEntityId, true);
    fetchEntity(m_config.lightEntityId, false);
    snapshot.latencyMs = std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - startedAtMs);
    snapshot.summary = snapshot.success ? QStringLiteral("Smart room state updated") : QStringLiteral("Smart room unavailable");
    return snapshot;
}

void HomeAssistantSmartHomeAdapter::executeLightCommand(const SmartLightCommand &command,
                                                        std::function<void(const SmartLightCommandResult &)> callback)
{
    callback(executeLightCommandBlocking(command));
}

SmartLightCommandResult HomeAssistantSmartHomeAdapter::executeLightCommandBlocking(const SmartLightCommand &command)
{
    SmartLightCommandResult result;
    QString configError;
    if (!hasUsableConfig(&configError) || m_config.lightEntityId.trimmed().isEmpty()) {
        result.errorKind = QStringLiteral("config");
        result.summary = QStringLiteral("Smart light unavailable");
        result.detail = configError.isEmpty() ? QStringLiteral("Light entity id is not configured.") : configError;
        return result;
    }

    QString service = QStringLiteral("turn_on");
    QJsonObject body{{QStringLiteral("entity_id"), m_config.lightEntityId}};
    if (command.action == QStringLiteral("turn_light_off")) {
        service = QStringLiteral("turn_off");
    } else if (command.action == QStringLiteral("set_light_brightness")) {
        body.insert(QStringLiteral("brightness_pct"), std::clamp(command.brightnessPercent, 1, 100));
    } else if (command.action == QStringLiteral("set_light_color")) {
        const QString color = command.colorName.trimmed().toLower();
        if (color == QStringLiteral("warm")) {
            body.insert(QStringLiteral("color_temp_kelvin"), 2700);
        } else if (color == QStringLiteral("white")) {
            body.insert(QStringLiteral("color_temp_kelvin"), 4000);
        } else if (!color.isEmpty()) {
            body.insert(QStringLiteral("color_name"), color);
        }
    }

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    const BlockingHttpResult http = blockingRequest(
        buildRequest(serviceUrl(QStringLiteral("light"), service), true),
        payload,
        true,
        m_config.requestTimeoutMs);
    result.httpStatus = http.httpStatus;
    result.latencyMs = http.latencyMs;
    result.success = http.ok;
    if (!http.ok) {
        result.errorKind = http.httpStatus == 401 || http.httpStatus == 403 ? QStringLiteral("auth") : QStringLiteral("transport");
        result.summary = QStringLiteral("Smart light command failed");
        result.detail = http.error;
        return result;
    }

    if (service == QStringLiteral("turn_off")) {
        result.summary = QStringLiteral("Light turned off.");
    } else if (command.action == QStringLiteral("set_light_brightness")) {
        result.summary = QStringLiteral("Light brightness set to %1%.").arg(std::clamp(command.brightnessPercent, 1, 100));
    } else if (command.action == QStringLiteral("set_light_color")) {
        result.summary = command.colorName.trimmed().isEmpty()
            ? QStringLiteral("Light color updated.")
            : QStringLiteral("Light set to %1.").arg(command.colorName.trimmed().toLower());
    } else {
        result.summary = QStringLiteral("Light turned on.");
    }
    result.detail = result.summary;
    return result;
}

SmartPresenceSnapshot HomeAssistantSmartHomeAdapter::parsePresenceState(const QJsonObject &stateObject,
                                                                        const QString &entityId,
                                                                        qint64 observedAtMs)
{
    SmartPresenceSnapshot snapshot;
    snapshot.sensorId = entityId;
    snapshot.source = QStringLiteral("home_assistant");
    snapshot.observedAtMs = observedAtMs;
    snapshot.rawState = stateObject.value(QStringLiteral("state")).toString();
    snapshot.available = !isUnavailableState(snapshot.rawState);
    snapshot.occupied = snapshot.available && isTruthyPresenceState(snapshot.rawState);
    snapshot.stale = !snapshot.available;
    return snapshot;
}

SmartLightSnapshot HomeAssistantSmartHomeAdapter::parseLightState(const QJsonObject &stateObject,
                                                                  const QString &entityId,
                                                                  qint64 observedAtMs)
{
    SmartLightSnapshot snapshot;
    snapshot.lightId = entityId;
    snapshot.source = QStringLiteral("home_assistant");
    snapshot.observedAtMs = observedAtMs;
    snapshot.rawState = stateObject.value(QStringLiteral("state")).toString();
    snapshot.available = !isUnavailableState(snapshot.rawState);
    snapshot.on = snapshot.rawState.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0;
    const QJsonObject attributes = stateObject.value(QStringLiteral("attributes")).toObject();
    snapshot.brightnessPercent = brightnessPercentFromAttributes(attributes);
    snapshot.colorSupported = hasColorSupport(attributes);
    snapshot.colorName = colorNameFromAttributes(attributes);
    snapshot.stale = !snapshot.available;
    return snapshot;
}

QUrl HomeAssistantSmartHomeAdapter::stateUrl(const QString &entityId) const
{
    return QUrl(trimmedBaseUrl(m_config.homeAssistantBaseUrl) + QStringLiteral("/api/states/") + entityId.trimmed());
}

QUrl HomeAssistantSmartHomeAdapter::serviceUrl(const QString &domain, const QString &service) const
{
    return QUrl(trimmedBaseUrl(m_config.homeAssistantBaseUrl)
                + QStringLiteral("/api/services/")
                + domain.trimmed()
                + QLatin1Char('/')
                + service.trimmed());
}

QNetworkRequest HomeAssistantSmartHomeAdapter::buildRequest(const QUrl &url, bool jsonBody) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VAXIL/1.0 SmartHome"));
    if (jsonBody) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    }
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(bearerToken()).toUtf8());
    return request;
}

QString HomeAssistantSmartHomeAdapter::bearerToken() const
{
    const QString envName = m_config.homeAssistantTokenEnvVar.trimmed().isEmpty()
        ? QStringLiteral("VAXIL_HOME_ASSISTANT_TOKEN")
        : m_config.homeAssistantTokenEnvVar.trimmed();
    return QProcessEnvironment::systemEnvironment().value(envName).trimmed();
}

bool HomeAssistantSmartHomeAdapter::hasUsableConfig(QString *detail) const
{
    if (!m_config.enabled) {
        if (detail) {
            *detail = QStringLiteral("Smart home integration is disabled.");
        }
        return false;
    }
    if (m_config.provider != QStringLiteral("home_assistant")) {
        if (detail) {
            *detail = QStringLiteral("Smart home provider is not home_assistant.");
        }
        return false;
    }
    if (m_config.homeAssistantBaseUrl.trimmed().isEmpty()) {
        if (detail) {
            *detail = QStringLiteral("Home Assistant base URL is not configured.");
        }
        return false;
    }
    if (bearerToken().isEmpty()) {
        if (detail) {
            *detail = QStringLiteral("Home Assistant token environment variable is empty.");
        }
        return false;
    }
    return true;
}
