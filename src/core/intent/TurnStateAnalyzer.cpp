#include "core/intent/TurnStateAnalyzer.h"

#include <QRegularExpression>

namespace {
bool containsAnyWholePhrase(const QString &input, const QStringList &phrases)
{
    for (const QString &phrase : phrases) {
        QString pattern = QRegularExpression::escape(phrase);
        pattern.replace(QStringLiteral("\\ "), QStringLiteral("\\s+"));
        const QRegularExpression regex(QStringLiteral("(^|\\b)%1(\\b|$)").arg(pattern));
        if (regex.match(input).hasMatch()) {
            return true;
        }
    }
    return false;
}
}

TurnState TurnStateAnalyzer::analyze(const TurnStateInput &input) const
{
    TurnState state;
    const QString lowered = input.normalizedInput.trimmed().toLower();

    state.isConfirmationReply = input.hasPendingConfirmation
        && containsAnyWholePhrase(lowered, {
               QStringLiteral("yes"),
               QStringLiteral("yeah"),
               QStringLiteral("yep"),
               QStringLiteral("go ahead"),
               QStringLiteral("continue"),
               QStringLiteral("no"),
               QStringLiteral("cancel"),
               QStringLiteral("stop")
           });

    state.isCorrection = containsAnyWholePhrase(lowered, {
        QStringLiteral("i mean"),
        QStringLiteral("that's wrong"),
        QStringLiteral("that is wrong"),
        QStringLiteral("not that"),
        QStringLiteral("no,")
    });

    state.refersToPreviousTask = input.hasUsableActionThread
        && (input.turnSignals.hasContinuationCue
            || input.turnSignals.hasContextReference
            || containsAnyWholePhrase(lowered, {
                   QStringLiteral("it"),
                   QStringLiteral("that"),
                   QStringLiteral("previous"),
                   QStringLiteral("result"),
                   QStringLiteral("what happened")
               }));

    state.isContinuation = state.refersToPreviousTask || input.turnSignals.hasContinuationCue;
    state.isNewTurn = !state.isContinuation && !state.isConfirmationReply;

    if (state.isConfirmationReply) {
        state.reasonCodes.push_back(QStringLiteral("turn_state.confirmation_reply"));
    }
    if (state.isCorrection) {
        state.reasonCodes.push_back(QStringLiteral("turn_state.correction"));
    }
    if (state.refersToPreviousTask) {
        state.reasonCodes.push_back(QStringLiteral("turn_state.refers_previous_task"));
    }
    if (state.isContinuation) {
        state.reasonCodes.push_back(QStringLiteral("turn_state.continuation"));
    }
    if (state.isNewTurn) {
        state.reasonCodes.push_back(QStringLiteral("turn_state.new_turn"));
    }

    return state;
}
