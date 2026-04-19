#include "core/StartupReadinessPolicy.h"

StartupReadinessResult StartupReadinessPolicy::evaluate(const StartupReadinessInput &input)
{
    StartupReadinessResult result;

    if (!input.initialSetupCompleted) {
        result.blocked = true;
        result.issue = QStringLiteral("Initial setup is incomplete.");
        return result;
    }
    if (input.chatBackendEndpoint.trimmed().isEmpty()) {
        result.blocked = true;
        result.issue = QStringLiteral("Local AI backend endpoint is missing.");
        return result;
    }
    if (input.whisperExecutable.trimmed().isEmpty() || !input.whisperExecutableExists) {
        result.blocked = true;
        result.issue = QStringLiteral("Whisper executable is missing.");
        return result;
    }
    if (input.whisperModelPath.trimmed().isEmpty() || !input.whisperModelExists) {
        result.blocked = true;
        result.issue = QStringLiteral("Whisper model is missing.");
        return result;
    }
    if (input.piperExecutable.trimmed().isEmpty() || !input.piperExecutableExists) {
        result.blocked = true;
        result.issue = QStringLiteral("Piper executable is missing.");
        return result;
    }
    if (input.piperVoiceModel.trimmed().isEmpty() || !input.piperVoiceModelExists) {
        result.blocked = true;
        result.issue = QStringLiteral("Piper voice model is missing.");
        return result;
    }
    if (input.ffmpegExecutable.trimmed().isEmpty() || !input.ffmpegExecutableExists) {
        result.blocked = true;
        result.issue = QStringLiteral("FFmpeg executable is missing.");
        return result;
    }

    if (input.wakeEngineRequired) {
        if (input.wakeRuntimePath.trimmed().isEmpty()) {
            result.blocked = true;
            result.issue = QStringLiteral("Wake runtime is missing.");
            return result;
        }
        if (input.wakeModelPath.trimmed().isEmpty()) {
            result.blocked = true;
            result.issue = QStringLiteral("Wake model is missing.");
            return result;
        }
    }

    if (!input.modelCatalogResolved) {
        result.blocked = false;
        result.issue = QStringLiteral("Loading local AI backend...");
        return result;
    }

    if (!input.availability.online) {
        result.blocked = true;
        result.issue = input.availability.status.trimmed().isEmpty()
            ? QStringLiteral("Local AI backend is offline.")
            : input.availability.status;
        return result;
    }
    if (!input.availability.modelAvailable) {
        result.blocked = true;
        result.issue = input.availability.status.trimmed().isEmpty()
            ? QStringLiteral("Selected model is unavailable.")
            : input.availability.status;
        return result;
    }

    if (input.wakeEngineRequired) {
        if (!input.wakeStartRequested) {
            result.blocked = false;
            result.issue = QStringLiteral("Starting wake engine...");
            return result;
        }
        if (!input.wakeEngineError.trimmed().isEmpty()) {
            result.blocked = true;
            result.issue = input.wakeEngineError;
            return result;
        }
        if (!input.wakeEngineReady) {
            result.blocked = false;
            result.issue = QStringLiteral("Starting wake engine...");
            return result;
        }
    }

    result.blocked = false;
    result.ready = true;
    return result;
}
