#include "tts/TtsEngineFactory.h"

#include "tts/PiperTtsEngine.h"
#include "tts/QwenTtsEngine.h"
#include "tts/SelectableTtsEngine.h"

TtsEngine *TtsEngineFactory::create(AppSettings *settings, LoggingService *loggingService, QObject *parent)
{
    auto *piperEngine = new PiperTtsEngine(settings, loggingService, parent);
    auto *qwenEngine = new QwenTtsEngine(settings, loggingService, parent);
    return new SelectableTtsEngine(settings, loggingService, piperEngine, qwenEngine, parent);
}
