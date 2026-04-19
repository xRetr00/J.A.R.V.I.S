#pragma once

#include <optional>

#include <QList>
#include <QString>
#include <QVariantMap>

#include "core/AssistantTypes.h"

enum class ActionThreadSelectionKind
{
    FreshTask,
    Attach,
    AskClarification,
    AuditOnlyCanceled,
    RetryFailed,
    PrivateContextBlocked
};

struct ActionThreadSelectionInput
{
    QString userInput;
    InputRouteDecision routeDecision;
    QList<ActionThread> recentThreads;
    qint64 nowMs = 0;
    bool privateMode = false;
    QVariantMap desktopContext;
};

struct ActionThreadSelectionResult
{
    ActionThreadSelectionKind kind = ActionThreadSelectionKind::FreshTask;
    std::optional<ActionThread> thread;
    QList<ActionThread> ambiguousThreads;
    QString reasonCode;
    QString userMessage;

    [[nodiscard]] bool shouldAttach() const
    {
        return kind == ActionThreadSelectionKind::Attach
            || kind == ActionThreadSelectionKind::RetryFailed;
    }
};

class ActionThreadSelectionPolicy
{
public:
    [[nodiscard]] static ActionThreadSelectionResult select(const ActionThreadSelectionInput &input);
};
