#include "ai/ReasoningRouter.h"

#include "core/intent/CommandCueMatcher.h"

ReasoningRouter::ReasoningRouter(QObject *parent)
    : QObject(parent)
    , m_commandKeywords({
          QStringLiteral("turn on"),
          QStringLiteral("turn off"),
          QStringLiteral("open"),
          QStringLiteral("close"),
          QStringLiteral("start"),
          QStringLiteral("stop"),
          QStringLiteral("launch"),
          QStringLiteral("mute"),
          QStringLiteral("unmute"),
          QStringLiteral("enable"),
          QStringLiteral("disable")
      })
    , m_analysisKeywords({
          QStringLiteral("analyze"),
          QStringLiteral("analysis"),
          QStringLiteral("explain"),
          QStringLiteral("why"),
          QStringLiteral("strategy"),
          QStringLiteral("compare"),
          QStringLiteral("reason"),
          QStringLiteral("tradeoff")
      })
{
}

ReasoningMode ReasoningRouter::chooseMode(const QString &input, bool autoRoutingEnabled, ReasoningMode defaultMode) const
{
    if (!autoRoutingEnabled) {
        return defaultMode;
    }

    if (isLikelyCommand(input)) {
        return ReasoningMode::Fast;
    }

    if (isDeepAnalysis(input)) {
        return ReasoningMode::Deep;
    }

    const auto trimmed = input.trimmed();
    if (trimmed.size() < 60 && !trimmed.contains(QChar::fromLatin1('?'))) {
        return ReasoningMode::Fast;
    }

    return ReasoningMode::Balanced;
}

bool ReasoningRouter::isLikelyCommand(const QString &input) const
{
    return CommandCueMatcher::hasStructuredCommandCue(input);
}

bool ReasoningRouter::isDeepAnalysis(const QString &input) const
{
    const auto lowered = input.toLower();
    for (const auto &keyword : m_analysisKeywords) {
        if (lowered.contains(keyword)) {
            return true;
        }
    }

    return false;
}
