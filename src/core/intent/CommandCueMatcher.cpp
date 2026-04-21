#include "core/intent/CommandCueMatcher.h"

#include <QRegularExpression>

namespace {
QString normalizeSpaces(const QString &input)
{
    QString lowered = input.trimmed().toLower();
    lowered.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return lowered;
}

QString stripLeadingSocialAndPolitePrefixes(const QString &input)
{
    QString candidate = normalizeSpaces(input);

    const QList<QRegularExpression> stripPatterns = {
        QRegularExpression(QStringLiteral("^(hi|hello|hey)\\b[\\s,;:!-]*")),
        QRegularExpression(QStringLiteral("^(please)\\b\\s*")),
        QRegularExpression(QStringLiteral("^(can you|could you|would you)\\b\\s*")),
        QRegularExpression(QStringLiteral("^(please can you|please could you|please would you)\\b\\s*"))
    };

    bool changed = true;
    while (changed && !candidate.isEmpty()) {
        changed = false;
        for (const QRegularExpression &pattern : stripPatterns) {
            const QRegularExpressionMatch match = pattern.match(candidate);
            if (!match.hasMatch()) {
                continue;
            }
            candidate = candidate.mid(match.capturedLength()).trimmed();
            changed = true;
        }
    }

    return candidate;
}
}

bool CommandCueMatcher::hasStructuredCommandCue(const QString &input)
{
    const QString stripped = stripLeadingSocialAndPolitePrefixes(input);
    if (stripped.isEmpty()) {
        return false;
    }

    static const QRegularExpression commandStartRegex(
        QStringLiteral("^(turn on|turn off|open|close|start|stop|launch|mute|unmute|enable|disable)\\b"));
    return commandStartRegex.match(stripped).hasMatch();
}
