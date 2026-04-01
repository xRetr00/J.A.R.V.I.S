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
        const MemoryContext &memory,
        const AssistantIdentity &identity,
        const UserProfile &userProfile,
        ResponseMode responseMode,
        const QString &sessionGoal,
        const QString &nextStepHint,
        ReasoningMode mode,
        const QString &visionContext = QString()) const;

    QList<AiMessage> buildCommandMessages(
        const QString &input,
        const AssistantIdentity &identity,
        const UserProfile &userProfile,
        ResponseMode responseMode,
        const QString &sessionGoal,
        ReasoningMode mode) const;

    QList<AiMessage> buildHybridAgentMessages(
        const QString &input,
        const MemoryContext &memory,
        const AssistantIdentity &identity,
        const UserProfile &userProfile,
        const QString &workspaceRoot,
        IntentType intent,
        const QList<AgentToolSpec> &availableTools,
        ResponseMode responseMode,
        const QString &sessionGoal,
        const QString &nextStepHint,
        ReasoningMode mode,
        const QString &visionContext = QString()) const;

    QList<AiMessage> buildHybridAgentContinuationMessages(
        const QString &input,
        const QList<AgentToolResult> &results,
        const MemoryContext &memory,
        const AssistantIdentity &identity,
        const UserProfile &userProfile,
        const QString &workspaceRoot,
        IntentType intent,
        const QList<AgentToolSpec> &availableTools,
        ResponseMode responseMode,
        const QString &sessionGoal,
        const QString &nextStepHint,
        ReasoningMode mode,
        const QString &visionContext = QString()) const;

    QString buildAgentInstructions(
        const MemoryContext &memory,
        const QList<SkillManifest> &skills,
        const QList<AgentToolSpec> &availableTools,
        const AssistantIdentity &identity,
        const UserProfile &userProfile,
        const QString &workspaceRoot,
        IntentType intent,
        bool memoryAutoWrite,
        ResponseMode responseMode,
        const QString &sessionGoal,
        const QString &nextStepHint,
        const QString &visionContext = QString()) const;

    QList<AgentToolSpec> getRelevantTools(const QString &input,
                                          IntentType intent,
                                          const QList<AgentToolSpec> &availableTools) const;
    QString buildAgentWorldContext(
        IntentType intent,
        const QList<AgentToolSpec> &availableTools,
        const MemoryContext &memory,
        const QString &workspaceRoot,
        const QString &visionContext = QString()) const;
    QString applyReasoningMode(const QString &input, ReasoningMode mode) const;

private:
    QString buildToolSchemaContext(const QList<AgentToolSpec> &tools) const;
    QString buildWorkspaceContext(const QString &workspaceRoot) const;
    QString buildLogsContext(const QString &workspaceRoot) const;
    QString buildCapabilityRulesContext(IntentType intent) const;
    QString buildFewShotExamples(IntentType intent) const;
    QString buildMemorySummary(const MemoryContext &memory) const;
};
