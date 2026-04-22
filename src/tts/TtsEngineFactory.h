#pragma once

class AppSettings;
class LoggingService;
class QObject;
class TtsEngine;

namespace TtsEngineFactory {

TtsEngine *create(AppSettings *settings, LoggingService *loggingService, QObject *parent = nullptr);

}
