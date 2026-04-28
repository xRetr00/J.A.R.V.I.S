#pragma once

#include <functional>
#include <algorithm>
#include <optional>

#include <QObject>
#include <QRegularExpression>
#include <QString>

#include "smart_home/SmartHomeTypes.h"

class AppSettings;
class LoggingService;

struct LocalSmartHomeIntent
{
    QString action;
    int brightnessPercent = -1;
    QString colorName;
    bool backendBypassed = true;
};

struct LocalSmartHomeRouteResult
{
    bool handled = false;
    bool success = false;
    QString action;
    QString response;
    QString status;
    qint64 latencyMs = 0;
};

class LocalSmartHomeIntentRouter final : public QObject
{
    Q_OBJECT

public:
    explicit LocalSmartHomeIntentRouter(AppSettings *settings, LoggingService *loggingService, QObject *parent = nullptr);

    static std::optional<LocalSmartHomeIntent> match(const QString &input)
    {
        QString value = input.trimmed().toLower();
        value.remove(QRegularExpression(QStringLiteral("[?!.,]")));
        value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        if (value.isEmpty()
            || value.contains(QStringLiteral("scene"))
            || value.contains(QStringLiteral("movie"))
            || value.contains(QStringLiteral("relaxing"))
            || value.contains(QStringLiteral("if someone"))
            || value.contains(QStringLiteral("when someone"))
            || value.contains(QStringLiteral("notify"))
            || value.contains(QStringLiteral("automation"))
            || value.contains(QStringLiteral("routine"))) {
            return std::nullopt;
        }

        LocalSmartHomeIntent intent;
        if (value == QStringLiteral("turn on the light") || value == QStringLiteral("turn light on")) {
            intent.action = QStringLiteral("turn_light_on");
            return intent;
        }
        if (value == QStringLiteral("turn off the light") || value == QStringLiteral("turn light off")) {
            intent.action = QStringLiteral("turn_light_off");
            return intent;
        }

        const QRegularExpression brightnessPattern(QStringLiteral("(?:set )?(?:the )?(?:light )?brightness(?: to)? (\\d{1,3}) ?%"));
        const QRegularExpressionMatch brightnessMatch = brightnessPattern.match(value);
        if (brightnessMatch.hasMatch()) {
            intent.action = QStringLiteral("set_light_brightness");
            intent.brightnessPercent = std::clamp(brightnessMatch.captured(1).toInt(), 1, 100);
            return intent;
        }

        const QRegularExpression colorPattern(QStringLiteral("(?:make|set)(?: the)? light (red|blue|warm|white)"));
        const QRegularExpressionMatch colorMatch = colorPattern.match(value);
        if (colorMatch.hasMatch()) {
            intent.action = QStringLiteral("set_light_color");
            intent.colorName = colorMatch.captured(1);
            return intent;
        }

        if (value == QStringLiteral("is the light on")) {
            intent.action = QStringLiteral("get_light_status");
            return intent;
        }
        if (value == QStringLiteral("what is the room status")
            || value == QStringLiteral("what is the smart room status")
            || value == QStringLiteral("is the room occupied")
            || value == QStringLiteral("is there anyone in the room")
            || value == QStringLiteral("am i home")
            || value == QStringLiteral("is my phone detected")
            || value == QStringLiteral("am i in the room")) {
            intent.action = QStringLiteral("get_room_status");
            return intent;
        }
        if (value == QStringLiteral("did anyone enter my room while i was away")
            || value == QStringLiteral("was anyone in my room")
            || value == QStringLiteral("did the room sensor detect someone while i was gone")) {
            intent.action = QStringLiteral("get_unknown_occupant_status");
            return intent;
        }

        return std::nullopt;
    }
    bool handle(const QString &input, std::function<void(const LocalSmartHomeRouteResult &)> callback);

private:
    SmartHomeConfig configFromSettings() const;
    LocalSmartHomeRouteResult answerFromSnapshot(const LocalSmartHomeIntent &intent, qint64 startedAtMs) const;
    QString summarizeRoomStatus(const SmartHomeSnapshot &snapshot, const LocalSmartHomeIntent &intent) const;
    QString summarizeLightStatus(const SmartHomeSnapshot &snapshot) const;
    void logRoute(const LocalSmartHomeRouteResult &result) const;

    AppSettings *m_settings = nullptr;
    LoggingService *m_loggingService = nullptr;
};
