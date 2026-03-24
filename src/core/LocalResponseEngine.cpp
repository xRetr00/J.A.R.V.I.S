#include "core/LocalResponseEngine.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace {
QHash<QString, QStringList> defaultResponses()
{
    return {
        {QStringLiteral("greeting_morning"), {
             QStringLiteral("Good morning sir, How can I assist Mr {user_name}?"),
             QStringLiteral("Good morning sir, How Is The Morning Boner {user_name}?"),
             QStringLiteral("Morning, {user_name}. How may I help?")
         }},
        {QStringLiteral("greeting_evening"), {
             QStringLiteral("Good evening, {user_name}. How can I assist?"),
             QStringLiteral("Good evening, {user_name}. What do you need?"),
             QStringLiteral("Evening, {user_name}. How may I help?")
         }},
        {QStringLiteral("small_talk"), {
             QStringLiteral("I'm here not there, {user_name}."),
             QStringLiteral("Ready when you are, {user_name}."),
             QStringLiteral("How can I help, {user_name}?")
         }},
        {QStringLiteral("wakeword_ready"), {
             QStringLiteral("Yes All Systems Is Operational And We Ready To Explore Mars , {user_name}."),
             QStringLiteral("Standing by or Not Yet Maybe Yes, {user_name}."),
             QStringLiteral("Dishwasher Is Ready, {user_name}?")
         }},
        {QStringLiteral("time_status"), {
             QStringLiteral("It is {current_time}, {user_name}."),
             QStringLiteral("The current time is {current_time} based on qantum clock in my back pocket, {user_name}."),
             QStringLiteral("It is now {current_time}, Sir.")
         }},
        {QStringLiteral("date_status"), {
             QStringLiteral("Today is {current_date}, Sir."),
             QStringLiteral("The date is {current_date}, Sir."),
             QStringLiteral("It is {current_date} today, Sir.")
         }},
        {QStringLiteral("ai_offline"), {
             QStringLiteral("I'm currently unable to reach the AI core, Sir."),
             QStringLiteral("My processing unit is offline at the moment, Sir."),
             QStringLiteral("I can't reach the AI core right now, Sir.")
         }},
        {QStringLiteral("error_timeout"), {
             QStringLiteral("The AI core did not answer in time, Sir."),
             QStringLiteral("That took too long to complete, Sir."),
             QStringLiteral("The request timed out, Sir.")
         }},
        {QStringLiteral("acknowledgement"), {
             QStringLiteral("Understood, Sir. Working on {target}."),
             QStringLiteral("All right, Sir. Handling {target}."),
             QStringLiteral("Done, Sir. {target} is handled.")
         }}
    };
}

QString sanitizeLocalResponse(QString text)
{
    static const QStringList forbiddenFragments = {
        QStringLiteral("tone"),
        QStringLiteral("addressing style"),
        QStringLiteral("system prompt"),
        QStringLiteral("internal prompt"),
        QStringLiteral("primary goals"),
        QStringLiteral("response contract"),
        QStringLiteral("local systems remain active"),
        QStringLiteral("operating normally"),
        QStringLiteral("running smoothly")
    };

    const QString lowered = text.toLower();
    for (const QString &fragment : forbiddenFragments) {
        if (lowered.contains(fragment)) {
            return QStringLiteral("How can I assist?");
        }
    }

    return text.simplified();
}
}

LocalResponseEngine::LocalResponseEngine(QObject *parent)
    : QObject(parent)
{
}

bool LocalResponseEngine::initialize()
{
    m_responses = defaultResponses();

    QFile file(QStringLiteral(":/qt/qml/JARVIS/gui/data/local_responses.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return !m_responses.isEmpty();
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto root = document.object();
    m_responses.clear();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QStringList variants;
        for (const auto &entry : it.value().toArray()) {
            variants.push_back(entry.toString());
        }
        m_responses.insert(it.key(), variants);
    }

    return !m_responses.isEmpty();
}

QString LocalResponseEngine::respondToIntent(LocalIntent intent, const LocalResponseContext &context)
{
    return renderTemplate(chooseVariant(resolveGroup(intent, context)), context);
}

QString LocalResponseEngine::respondToError(const QString &errorKey, const LocalResponseContext &context)
{
    return renderTemplate(chooseVariant(errorKey), context);
}

QString LocalResponseEngine::acknowledgement(const QString &target, const LocalResponseContext &context)
{
    return renderTemplate(chooseVariant(QStringLiteral("acknowledgement")), context, target);
}

QString LocalResponseEngine::wakeWordReady(const LocalResponseContext &context)
{
    return renderTemplate(chooseVariant(QStringLiteral("wakeword_ready")), context);
}

QString LocalResponseEngine::currentTimeResponse(const LocalResponseContext &context)
{
    return renderTemplate(chooseVariant(QStringLiteral("time_status")), context);
}

QString LocalResponseEngine::currentDateResponse(const LocalResponseContext &context)
{
    return renderTemplate(chooseVariant(QStringLiteral("date_status")), context);
}

QString LocalResponseEngine::resolveGroup(LocalIntent intent, const LocalResponseContext &context) const
{
    switch (intent) {
    case LocalIntent::Greeting:
        if (context.timeOfDay == QStringLiteral("morning")) {
            return QStringLiteral("greeting_morning");
        }
        if (context.timeOfDay == QStringLiteral("evening")) {
            return QStringLiteral("greeting_evening");
        }
        return QStringLiteral("small_talk");
    case LocalIntent::SmallTalk:
        return QStringLiteral("small_talk");
    default:
        return QStringLiteral("small_talk");
    }
}

QString LocalResponseEngine::renderTemplate(const QString &variant, const LocalResponseContext &context, const QString &target) const
{
    QString text = variant;
    const QString fallbackName = context.userName.isEmpty() ? QStringLiteral("sir") : context.userName;
    text.replace(QStringLiteral("{assistant_name}"), context.assistantName);
    text.replace(QStringLiteral("{user_name}"), fallbackName);
    text.replace(QStringLiteral("{time_of_day}"), context.timeOfDay);
    text.replace(QStringLiteral("{system_state}"), context.systemState);
    text.replace(QStringLiteral("{tone}"), context.tone);
    text.replace(QStringLiteral("{addressing_style}"), context.addressingStyle);
    text.replace(QStringLiteral("{target}"), target);
    text.replace(QStringLiteral("{current_time}"), context.currentTime);
    text.replace(QStringLiteral("{current_date}"), context.currentDate);
    text.replace(QStringLiteral("{wake_word}"), context.wakeWord);
    return sanitizeLocalResponse(text);
}

QString LocalResponseEngine::chooseVariant(const QString &group)
{
    const auto variants = m_responses.value(group);
    if (variants.isEmpty()) {
        return QStringLiteral("I am ready.");
    }

    if (variants.size() == 1) {
        return variants.first();
    }

    const int previousIndex = m_lastIndexByGroup.value(group, -1);
    int nextIndex = QRandomGenerator::global()->bounded(variants.size());
    if (variants.size() > 1 && nextIndex == previousIndex) {
        nextIndex = (nextIndex + 1) % variants.size();
    }

    m_lastIndexByGroup[group] = nextIndex;
    return variants.at(nextIndex);
}
