#pragma once

#include <QObject>

#include "core/AssistantTypes.h"

class PromptAdapter : public QObject
{
    Q_OBJECT

public:
    explicit PromptAdapter(QObject *parent = nullptr);

    QList<AiMessage> buildConversationMessages(
        const QString &input,
        const QList<AiMessage> &history,
        const QList<MemoryRecord> &memory,
        ReasoningMode mode) const;

    QList<AiMessage> buildCommandMessages(const QString &input, ReasoningMode mode) const;

    QString applyReasoningMode(const QString &input, ReasoningMode mode) const;
};
