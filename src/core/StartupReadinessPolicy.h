#pragma once

#include "core/AssistantTypes.h"

struct StartupReadinessInput
{
    bool initialSetupCompleted = false;

    QString chatBackendEndpoint;

    QString whisperExecutable;
    bool whisperExecutableExists = false;

    QString whisperModelPath;
    bool whisperModelExists = false;

    QString piperExecutable;
    bool piperExecutableExists = false;

    QString piperVoiceModel;
    bool piperVoiceModelExists = false;

    QString ffmpegExecutable;
    bool ffmpegExecutableExists = false;

    bool wakeEngineRequired = false;
    QString wakeRuntimePath;
    QString wakeModelPath;

    bool modelCatalogResolved = false;
    AiAvailability availability;

    bool wakeStartRequested = false;
    bool wakeEngineReady = false;
    QString wakeEngineError;
};

struct StartupReadinessResult
{
    bool ready = false;
    bool blocked = false;
    QString issue;
};

class StartupReadinessPolicy
{
public:
    [[nodiscard]] static StartupReadinessResult evaluate(const StartupReadinessInput &input);
};
