#pragma once

#include <QString>

#include "tts/TtsEngine.h"

class SpokenTextShaper
{
public:
    QString shape(const QString &input, const TtsUtteranceContext &context) const;
};

