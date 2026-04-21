#include "core/intent/TurnSignalExtractor.h"

#include <QRegularExpression>

#include "core/intent/CommandCueMatcher.h"

namespace {
QString normalizeForSignals(const QString &input)
{
    QString normalized = input.trimmed().toLower();
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized;
}

bool matchesAny(const QString &text, const QStringList &phrases, QStringList *matched = nullptr)
{
    for (const QString &phrase : phrases) {
        QString pattern = QRegularExpression::escape(phrase);
        pattern.replace(QStringLiteral("\\ "), QStringLiteral("\\s+"));
        const QRegularExpression regex(QStringLiteral("(^|\\b)%1(\\b|$)").arg(pattern));
        if (!regex.match(text).hasMatch()) {
            continue;
        }
        if (matched != nullptr) {
            matched->push_back(phrase);
        }
        return true;
    }
    return false;
}

bool startsWithAny(const QString &text, const QStringList &phrases, QStringList *matched = nullptr)
{
    for (const QString &phrase : phrases) {
        QString pattern = QRegularExpression::escape(phrase);
        pattern.replace(QStringLiteral("\\ "), QStringLiteral("\\s+"));
        const QRegularExpression regex(QStringLiteral("^\\s*%1(\\b|[,.!?]|\\s|$)").arg(pattern));
        if (!regex.match(text).hasMatch()) {
            continue;
        }
        if (matched != nullptr) {
            matched->push_back(phrase);
        }
        return true;
    }
    return false;
}
}

TurnSignals TurnSignalExtractor::extract(const QString &input) const
{
    TurnSignals extracted;
    extracted.rawInput = input;
    extracted.normalizedInput = normalizeForSignals(input);

    if (extracted.normalizedInput.isEmpty()) {
        return extracted;
    }

    const QStringList greetingCues = {
        QStringLiteral("hello"),
        QStringLiteral("hi"),
        QStringLiteral("hey"),
        QStringLiteral("good morning"),
        QStringLiteral("good afternoon"),
        QStringLiteral("good evening")
    };
    const QStringList smallTalkCues = {
        QStringLiteral("how are you"),
        QStringLiteral("what's up"),
        QStringLiteral("whats up"),
        QStringLiteral("how's it going"),
        QStringLiteral("nice to meet you"),
        QStringLiteral("good to see you"),
        QStringLiteral("thank you"),
        QStringLiteral("thanks")
    };
    const QStringList questionCues = {
        QStringLiteral("what"),
        QStringLiteral("why"),
        QStringLiteral("how"),
        QStringLiteral("explain"),
        QStringLiteral("compare")
    };
    const QStringList deterministicCues = {
        QStringLiteral("youtube"),
        QStringLiteral("browser"),
        QStringLiteral("settings"),
        QStringLiteral("timer"),
        QStringLiteral("open app"),
        QStringLiteral("launch app")
    };
    const QStringList contextReferenceCues = {
        QStringLiteral("this file"),
        QStringLiteral("current file"),
        QStringLiteral("this page"),
        QStringLiteral("current page"),
        QStringLiteral("that file"),
        QStringLiteral("that page")
    };
    const QStringList continuationCues = {
        QStringLiteral("open it"),
        QStringLiteral("read it"),
        QStringLiteral("from the result"),
        QStringLiteral("from the results"),
        QStringLiteral("what happened"),
        QStringLiteral("what did you do")
    };
    const QStringList negationCues = {
        QStringLiteral("no"),
        QStringLiteral("don't"),
        QStringLiteral("dont"),
        QStringLiteral("never"),
        QStringLiteral("stop")
    };
    const QStringList urgencyCues = {
        QStringLiteral("now"),
        QStringLiteral("urgent"),
        QStringLiteral("asap"),
        QStringLiteral("immediately")
    };

    extracted.hasGreeting = startsWithAny(extracted.normalizedInput, greetingCues, &extracted.matchedCues);
    extracted.hasSmallTalk = matchesAny(extracted.normalizedInput, smallTalkCues, &extracted.matchedCues);
    extracted.hasCommandCue = CommandCueMatcher::hasStructuredCommandCue(extracted.normalizedInput);
    extracted.hasQuestionCue = extracted.normalizedInput.contains(QChar::fromLatin1('?'))
        || startsWithAny(extracted.normalizedInput, questionCues, &extracted.matchedCues)
        || matchesAny(extracted.normalizedInput, questionCues, &extracted.matchedCues);
    extracted.hasDeterministicCue = matchesAny(extracted.normalizedInput, deterministicCues, &extracted.matchedCues);
    extracted.hasContextReference = matchesAny(extracted.normalizedInput, contextReferenceCues, &extracted.matchedCues);
    extracted.hasContinuationCue = matchesAny(extracted.normalizedInput, continuationCues, &extracted.matchedCues);
    extracted.hasNegation = matchesAny(extracted.normalizedInput, negationCues, &extracted.matchedCues);
    extracted.hasUrgency = matchesAny(extracted.normalizedInput, urgencyCues, &extracted.matchedCues);
    extracted.socialOnly = (extracted.hasGreeting || extracted.hasSmallTalk)
        && !extracted.hasQuestionCue
        && !extracted.hasCommandCue
        && !extracted.hasDeterministicCue
        && !extracted.hasContextReference
        && !extracted.hasContinuationCue;

    if (extracted.hasCommandCue) {
        extracted.matchedCues.push_back(QStringLiteral("command_start"));
    }

    return extracted;
}
