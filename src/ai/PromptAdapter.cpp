#include "ai/PromptAdapter.h"

PromptAdapter::PromptAdapter(QObject *parent)
    : QObject(parent)
{
}

QList<AiMessage> PromptAdapter::buildConversationMessages(
    const QString &input,
    const QList<AiMessage> &history,
    const QList<MemoryRecord> &memory,
    ReasoningMode mode) const
{
    QString systemPrompt =
        QStringLiteral("You are JARVIS, a calm, intelligent desktop AI assistant. "
                       "Respond with concise, confident, minimal verbosity. "
                       "Stay practical and directly useful.");

    if (!memory.isEmpty()) {
        systemPrompt += QStringLiteral("\nRelevant user memory:");
        for (const auto &record : memory) {
            systemPrompt += QStringLiteral("\n- %1: %2 = %3")
                                .arg(record.type, record.key, record.value);
        }
    }

    QList<AiMessage> messages{
        {.role = QStringLiteral("system"), .content = systemPrompt}
    };

    for (const auto &item : history) {
        messages.push_back(item);
    }

    messages.push_back({
        .role = QStringLiteral("user"),
        .content = applyReasoningMode(input, mode)
    });

    return messages;
}

QList<AiMessage> PromptAdapter::buildCommandMessages(const QString &input, ReasoningMode mode) const
{
    return {
        {
            .role = QStringLiteral("system"),
            .content =
                QStringLiteral("You extract desktop assistant commands. "
                               "Return strict JSON only with keys: intent, target, action, confidence, args. "
                               "Use confidence from 0.0 to 1.0. If uncertain, return intent as \"unknown\".")
        },
        {
            .role = QStringLiteral("user"),
            .content = applyReasoningMode(input, mode)
        }
    };
}

QString PromptAdapter::applyReasoningMode(const QString &input, ReasoningMode mode) const
{
    switch (mode) {
    case ReasoningMode::Fast:
        return input + QStringLiteral(" /no_think");
    case ReasoningMode::Deep:
        return QStringLiteral("Think step by step before answering.\n") + input;
    case ReasoningMode::Balanced:
    default:
        return input;
    }
}
