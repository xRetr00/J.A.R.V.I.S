#include "core/SpeechTranscriptGuard.h"

#include <algorithm>

#include <QRegularExpression>
#include <QSet>

#include "wakeword/WakeWordDetector.h"

namespace {
QString normalizeStageAnnotation(const QString &input)
{
    QString normalized = input.trimmed().toLower();
    normalized.remove(QRegularExpression(QStringLiteral("^[\\[(]+|[\\])]+$")));
    normalized.remove(QRegularExpression(QStringLiteral("[^a-z]")));
    return normalized;
}

bool isLikelyNonSpeechTranscript(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }

    const bool bracketed = (trimmed.startsWith(QChar::fromLatin1('[')) && trimmed.endsWith(QChar::fromLatin1(']')))
        || (trimmed.startsWith(QChar::fromLatin1('(')) && trimmed.endsWith(QChar::fromLatin1(')')));
    if (!bracketed) {
        return false;
    }

    const QString normalized = normalizeStageAnnotation(trimmed);
    return normalized == QStringLiteral("musicplaying")
        || normalized == QStringLiteral("applause")
        || normalized == QStringLiteral("laughter")
        || normalized == QStringLiteral("silence")
        || normalized == QStringLiteral("noise")
        || normalized == QStringLiteral("backgroundnoise")
        || normalized == QStringLiteral("inaudible");
}

bool isLikelySttArtifactTranscript(const QString &input)
{
    QString normalized = input.toLower();
    normalized.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral(" "));
    normalized = normalized.simplified();
    if (normalized.isEmpty()) {
        return true;
    }

    static const QStringList knownArtifacts = {
        QStringLiteral("transcribed by"),
        QStringLiteral("transcribe literally"),
        QStringLiteral("transcribed literally"),
        QStringLiteral("subtitle by"),
        QStringLiteral("subtitles by"),
        QStringLiteral("captions by"),
        QStringLiteral("thanks for watching"),
        QStringLiteral("learn english for free"),
        QStringLiteral("engvid"),
        QStringLiteral("subscribe for more"),
        QStringLiteral("follow for more")
    };

    for (const QString &phrase : knownArtifacts) {
        const QString escaped = QRegularExpression::escape(phrase);
        const QString pattern = QStringLiteral("(^|\\s)%1(\\s|$)").arg(escaped).replace(QStringLiteral("\\ "), QStringLiteral("\\s+"));
        if (QRegularExpression(pattern).match(normalized).hasMatch()) {
            return true;
        }
    }

    const QStringList words = normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (QRegularExpression(QStringLiteral("\\b(?:www\\s+)?[a-z0-9]+\\s+(?:com|net|org|io|ai)\\b")).match(normalized).hasMatch()) {
        return true;
    }
    if (words.size() <= 3) {
        for (const QString &word : words) {
            if (word.startsWith(QStringLiteral("transcrib"))
                || word.startsWith(QStringLiteral("subtitle"))
                || word.startsWith(QStringLiteral("caption"))) {
                return true;
            }
        }
    }

    return false;
}

QStringList transcriptWords(const QString &input)
{
    return input.toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

QString normalizePhrase(const QString &input)
{
    QString normalized = input.toLower();
    normalized.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral(" "));
    return normalized.simplified();
}

bool isLowSignalConversationToken(const QString &token)
{
    static const QSet<QString> lowSignalTokens = {
        QStringLiteral("you"),
        QStringLiteral("your"),
        QStringLiteral("it"),
        QStringLiteral("its"),
        QStringLiteral("this"),
        QStringLiteral("that"),
        QStringLiteral("there"),
        QStringLiteral("tip"),
        QStringLiteral("uh"),
        QStringLiteral("um"),
        QStringLiteral("hmm"),
        QStringLiteral("hm"),
        QStringLiteral("ah"),
        QStringLiteral("oh"),
        QStringLiteral("yeah"),
        QStringLiteral("yep")
    };
    return lowSignalTokens.contains(token);
}

bool allWordsLowSignal(const QStringList &words)
{
    if (words.isEmpty()) {
        return true;
    }

    for (const QString &word : words) {
        if (!isLowSignalConversationToken(word)) {
            return false;
        }
    }

    return true;
}

bool containsPhrase(const QString &normalizedInput, const QString &phrase)
{
    const QString escaped = QRegularExpression::escape(phrase);
    const QString pattern = QStringLiteral("(^|\\s)%1(\\s|$)").arg(escaped).replace(QStringLiteral("\\ "), QStringLiteral("\\s+"));
    return QRegularExpression(pattern).match(normalizedInput).hasMatch();
}

bool startsWithAllowedFollowUpWord(const QStringList &words)
{
    if (words.isEmpty()) {
        return false;
    }

    static const QSet<QString> allowedStarts = {
        QStringLiteral("what"),
        QStringLiteral("where"),
        QStringLiteral("when"),
        QStringLiteral("why"),
        QStringLiteral("how"),
        QStringLiteral("who"),
        QStringLiteral("can"),
        QStringLiteral("could"),
        QStringLiteral("would"),
        QStringLiteral("will"),
        QStringLiteral("should"),
        QStringLiteral("do"),
        QStringLiteral("does"),
        QStringLiteral("did"),
        QStringLiteral("is"),
        QStringLiteral("are"),
        QStringLiteral("am"),
        QStringLiteral("tell"),
        QStringLiteral("show"),
        QStringLiteral("list"),
        QStringLiteral("read"),
        QStringLiteral("write"),
        QStringLiteral("create"),
        QStringLiteral("open"),
        QStringLiteral("close"),
        QStringLiteral("start"),
        QStringLiteral("stop"),
        QStringLiteral("set"),
        QStringLiteral("search"),
        QStringLiteral("find"),
        QStringLiteral("remember"),
        QStringLiteral("forget"),
        QStringLiteral("save"),
        QStringLiteral("delete"),
        QStringLiteral("play"),
        QStringLiteral("pause"),
        QStringLiteral("resume"),
        QStringLiteral("make"),
        QStringLiteral("give"),
        QStringLiteral("call"),
        QStringLiteral("name"),
        QStringLiteral("check"),
        QStringLiteral("run"),
        QStringLiteral("explain"),
        QStringLiteral("summarize"),
        QStringLiteral("use"),
        QStringLiteral("go"),
        QStringLiteral("okay"),
        QStringLiteral("ok"),
        QStringLiteral("please"),
        QStringLiteral("thanks"),
        QStringLiteral("thank")
    };

    return allowedStarts.contains(words.first());
}

bool isConversationStopPhrase(const QString &input)
{
    const QString normalized = normalizePhrase(input);
    if (normalized.isEmpty()) {
        return false;
    }

    static const QStringList phrases = {
        QStringLiteral("stop"),
        QStringLiteral("stop listening"),
        QStringLiteral("stop talking"),
        QStringLiteral("sleep"),
        QStringLiteral("go to sleep"),
        QStringLiteral("sleep now"),
        QStringLiteral("shutdown"),
        QStringLiteral("shut down"),
        QStringLiteral("bye"),
        QStringLiteral("goodbye"),
        QStringLiteral("good bye"),
        QStringLiteral("thank you"),
        QStringLiteral("thanks"),
        QStringLiteral("no thanks"),
        QStringLiteral("never mind"),
        QStringLiteral("cancel"),
        QStringLiteral("that is all"),
        QStringLiteral("thats all"),
        QStringLiteral("that s all"),
        QStringLiteral("stand by"),
        QStringLiteral("standby")
    };

    for (const QString &phrase : phrases) {
        if (containsPhrase(normalized, phrase)) {
            return true;
        }
    }

    return false;
}

bool shouldIgnoreAmbiguousTranscript(const QString &transcript,
                                     const SpeechTranscriptGuardContext &context,
                                     bool wakePhraseDetected,
                                     bool stopPhraseDetected)
{
    const QStringList words = transcriptWords(transcript);
    if (words.isEmpty()) {
        return true;
    }

    const QString joined = words.join(QStringLiteral(" "));
    static const QStringList ambiguousPhrases = {
        QStringLiteral("you"),
        QStringLiteral("yeah"),
        QStringLiteral("yep"),
        QStringLiteral("uh"),
        QStringLiteral("um"),
        QStringLiteral("hmm"),
        QStringLiteral("hm"),
        QStringLiteral("ah"),
        QStringLiteral("oh"),
        QStringLiteral("i am now"),
        QStringLiteral("its a time"),
        QStringLiteral("it's a time")
    };
    if (ambiguousPhrases.contains(joined)) {
        return true;
    }

    if (wakePhraseDetected || stopPhraseDetected) {
        return false;
    }

    if (words.size() == 1) {
        const QString token = words.first();
        static const QStringList allowSingleWordCommands = {
            QStringLiteral("stop"),
            QStringLiteral("mute"),
            QStringLiteral("unmute"),
            QStringLiteral("open"),
            QStringLiteral("close"),
            QStringLiteral("start")
        };
        if (!allowSingleWordCommands.contains(token) && token.size() <= 3) {
            return true;
        }
    }

    if (context.conversationSessionActive && !startsWithAllowedFollowUpWord(words)) {
        if (allWordsLowSignal(words)) {
            return true;
        }

        if (words.size() <= 3
            && isLowSignalConversationToken(words.first())
            && allWordsLowSignal(words.mid(0, std::min<qsizetype>(words.size(), 3)))) {
            return true;
        }
    }

    return false;
}
}

SpeechTranscriptDecision SpeechTranscriptGuard::evaluate(const QString &transcript,
                                                         const SpeechTranscriptGuardContext &context) const
{
    SpeechTranscriptDecision decision;
    decision.wakePhraseDetected = WakeWordDetector::isWakeWordDetected(transcript);
    decision.stopPhraseDetected = isConversationStopPhrase(transcript);

    if (isLikelyNonSpeechTranscript(transcript)) {
        decision.disposition = SpeechTranscriptDisposition::IgnoreNonSpeech;
        return decision;
    }
    if (isLikelySttArtifactTranscript(transcript)) {
        decision.disposition = SpeechTranscriptDisposition::IgnoreSttArtifact;
        return decision;
    }
    if (shouldIgnoreAmbiguousTranscript(transcript, context, decision.wakePhraseDetected, decision.stopPhraseDetected)) {
        decision.disposition = SpeechTranscriptDisposition::IgnoreAmbiguous;
        return decision;
    }

    decision.disposition = SpeechTranscriptDisposition::Accept;
    return decision;
}

bool SpeechTranscriptGuard::isConversationStopPhrase(const QString &input) const
{
    return ::isConversationStopPhrase(input);
}
