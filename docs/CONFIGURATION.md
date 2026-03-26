# Configuration Guide

## Identity Files

### config/identity.json

Defines assistant persona:

- assistant_name
- personality
- tone
- addressing_style

### config/user_profile.json

Defines user identity and preferences:

- user_name: used for UI text and spoken addressing
- preferences: free-form object

## Runtime Settings File

AppSettings persists to:

- Qt AppDataLocation/settings.json

Key fields include:

General backend and routing:

- chatBackendKind
- chatBackendEndpoint / lmStudioEndpoint
- chatBackendModel / selectedModel
- defaultReasoningMode
- autoRoutingEnabled
- streamingEnabled
- requestTimeoutMs

Agent and tool-calling controls:

- agentEnabled
- agentProviderMode
- conversationTemperature
- conversationTopP
- toolUseTemperature
- providerTopK
- maxOutputTokens
- memoryAutoWrite
- webSearchProvider
- tracePanelEnabled

MCP integration controls:

- mcpEnabled
- mcpCatalogUrl
- mcpServerUrl

Speech recognition and intent:

- whisperExecutable
- whisperModelPath
- intentModelPath
- selectedIntentModelId

Speech synthesis and output:

- ttsEngineKind
- piperExecutable
- piperVoiceModel
- selectedVoicePresetId
- ffmpegExecutable
- voiceSpeed
- voicePitch

Wake and audio processing:

- wakeEngineKind
- wakeWordPhrase
- preciseEngineExecutable
- preciseModelPath
- preciseTriggerThreshold
- preciseTriggerCooldownMs
- aecEnabled
- rnnoiseEnabled
- vadSensitivity
- micSensitivity

Devices and UI:

- selectedAudioInputDeviceId / selectedAudioOutputDeviceId
- clickThroughEnabled
- initialSetupCompleted

## Guardrails and Value Ranges

Current clamps in code:

- preciseTriggerThreshold: 0.30 to 0.85
- preciseTriggerCooldownMs: 600 to 900 ms
- vadSensitivity: 0.05 to 0.95
- conversationTemperature: 0.0 to 2.0
- toolUseTemperature: 0.0 to 2.0
- maxOutputTokens: 64 to 8192
- voiceSpeed: 0.85 to 0.92
- voicePitch: 0.90 to 0.97

Current defaults:

- wakeEngineKind: sherpa-onnx
- ttsEngineKind: piper
- wakeWordPhrase: Jarvis
- requestTimeoutMs: 12000
- selectedIntentModelId: intent-minilm-int8

## Recommended Local Backend Setup

For LM Studio compatible mode:

- Endpoint: http://localhost:1234
- API paths used:
  - /v1/models
  - /v1/chat/completions

## Tool and Model Roots

ToolManager scans and writes under Qt AppDataLocation:

- tools root: `<AppDataLocation>/third_party`
- download cache: `<AppDataLocation>/tools`

Important downloadable entries include:

- onnxruntime
- sherpa-onnx
- sherpa-kws-model
- sentencepiece
- silero-vad-model
- intent-minilm-int8 / intent-minilm-q4f16 / intent-minilm-fp32
- rnnoise
- speexdsp

## Runtime Logs

- bin/logs/startup.log
- bin/logs/jarvis.log
- bin/logs/AI/*.log

If behavior looks wrong, inspect these logs first.
