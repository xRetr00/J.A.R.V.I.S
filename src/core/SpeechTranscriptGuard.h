#pragma once

#include <QString>

enum class SpeechTranscriptDisposition {
    Accept,
    IgnoreNonSpeech,
    IgnoreSttArtifact,
    IgnoreAmbiguous
};

struct SpeechTranscriptGuardContext
{
    bool conversationSessionActive = false;
};

struct SpeechTranscriptDecision
{
    SpeechTranscriptDisposition disposition = SpeechTranscriptDisposition::Accept;
    bool wakePhraseDetected = false;
    bool stopPhraseDetected = false;
    QString reasonCode;
};

class SpeechTranscriptGuard
{
public:
    [[nodiscard]] SpeechTranscriptDecision evaluate(const QString &transcript,
                                                    const SpeechTranscriptGuardContext &context) const;
    [[nodiscard]] bool isConversationStopPhrase(const QString &input) const;
};
