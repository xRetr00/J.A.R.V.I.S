#include "tts/SpokenTextShaper.h"

#include <QRegularExpression>

namespace {
QString collapseWhitespace(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

QString ensureTerminalPunctuation(QString text)
{
    text = collapseWhitespace(text);
    if (!text.isEmpty()
        && !text.endsWith(QChar::fromLatin1('.'))
        && !text.endsWith(QChar::fromLatin1('!'))
        && !text.endsWith(QChar::fromLatin1('?'))) {
        text += QChar::fromLatin1('.');
    }
    return text;
}

QString shortenToSpeakableLength(const QString &input)
{
    constexpr int kMaxSentences = 2;
    constexpr int kMaxChars = 220;

    const QStringList sentences = input.split(QRegularExpression(QStringLiteral("(?<=[.!?])\\s+")), Qt::SkipEmptyParts);
    QStringList selected;
    int chars = 0;
    bool truncated = false;

    for (const QString &rawSentence : sentences) {
        QString sentence = ensureTerminalPunctuation(rawSentence.trimmed());
        if (sentence.isEmpty()) {
            continue;
        }

        if (selected.size() >= kMaxSentences) {
            truncated = true;
            break;
        }

        const int nextChars = chars + (selected.isEmpty() ? 0 : 1) + sentence.size();
        if (nextChars > kMaxChars) {
            if (selected.isEmpty()) {
                sentence = ensureTerminalPunctuation(sentence.left(kMaxChars).trimmed());
                if (!sentence.isEmpty()) {
                    selected.push_back(sentence);
                }
            }
            truncated = true;
            break;
        }

        selected.push_back(sentence);
        chars = nextChars;
    }

    QString result = selected.join(QStringLiteral(" "));
    if (result.isEmpty()) {
        result = ensureTerminalPunctuation(input.left(kMaxChars).trimmed());
        truncated = input.size() > result.size();
    }

    if (truncated && !result.isEmpty()) {
        result = ensureTerminalPunctuation(result) + QStringLiteral(" The rest is on screen.");
    }

    return result;
}
} // namespace

QString SpokenTextShaper::shape(const QString &input, const TtsUtteranceContext &context) const
{
    QString shaped = input;
    if (shaped.trimmed().isEmpty()) {
        return {};
    }

    const bool assistantReply = context.utteranceClass.trimmed().toLower() == QStringLiteral("assistant_reply");
    if (!assistantReply) {
        return ensureTerminalPunctuation(shaped);
    }

    shaped.replace(QRegularExpression(QStringLiteral("(?m)^\\s*[-*+]\\s+")), QStringLiteral(""));
    shaped.replace(QRegularExpression(QStringLiteral("(?m)^\\s*\\d+\\.\\s+")), QStringLiteral(""));
    shaped.replace(QRegularExpression(QStringLiteral("\\bI don'?t have information (on|about) that\\b"),
                                      QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("I don't have that yet"));
    shaped.replace(QRegularExpression(QStringLiteral("\\bI (?:am|m) unable to\\b"),
                                      QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("I couldn't"));
    shaped.replace(QRegularExpression(QStringLiteral("\\s*\\n+\\s*")), QStringLiteral(". "));
    shaped = collapseWhitespace(shaped);
    shaped = shortenToSpeakableLength(shaped);

    return ensureTerminalPunctuation(shaped);
}

