# Architecture Overview

## Runtime Model

J.A.R.V.I.S is a tray-first desktop assistant with three main windows loaded by a single QQmlApplicationEngine:

- OverlayWindow: ambient orb, status, toast feedback
- SettingsWindow: full runtime configuration
- SetupWizard: first-run workflow

Core orchestration starts in src/app/JarvisApplication.cpp and wires all services through dependency injection.

## High-Level Flow

1. App bootstrap
   - Loads settings (AppSettings)
   - Loads identity/profile (IdentityProfileService)
   - Initializes logging (LoggingService)
2. Core controller startup
   - AssistantController initializes AI, STT, TTS, wake, memory, and routing services
3. UI binding
   - BackendFacade exposes controller/settings/profile properties and invokables to QML
4. Runtime interaction
   - Wake engine or manual trigger starts capture
   - STT transcribes audio
   - Intent router decides local response vs AI request
   - PromptAdapter builds contextual prompt
   - AI backend streams response
   - StreamAssembler emits sentence chunks
   - TTS speaks output and overlay reflects state

## Core Components

### Application Layer

- JarvisApplication:
  - Loads QML windows
  - Configures tray icon and hotkey
  - Coordinates setup-complete transition into wake monitoring

### Controller Layer

- AssistantController:
  - Central state machine for idle/listening/processing/speaking
  - Manages duplex rules and wake resume timing
  - Routes local responses and AI conversations
  - Persists settings changes through AppSettings

### AI Layer

- PromptAdapter:
  - Builds conversation and command extraction prompts
  - Injects identity, profile, preferences, and runtime context
- RuntimeAiBackendClient / OpenAiCompatibleClient / LmStudioClient:
  - OpenAI-compatible API handling
  - Model fetch and chat completions (streaming and non-streaming)
- ReasoningRouter:
  - Chooses fast/balanced/deep mode from input
- StreamAssembler:
  - Chunks stream into sentence-ready segments

### Voice Layer

- WakeWordEnginePrecise:
  - Streams mic audio to precise-engine
  - Uses moving average + consecutive frame gating for stable detection
- AudioInputService:
  - Captures PCM audio for transcription with VAD framing
- WhisperSttEngine:
  - Invokes whisper.cpp for transcription
- PiperTtsEngine / WorkerTtsEngine:
  - Local speech synthesis and output handling

### Settings and Identity

- AppSettings:
  - Reads/writes settings.json in Qt AppDataLocation
  - Applies clamps for sensitivity and threshold values
- IdentityProfileService:
  - Reads/writes config/identity.json and config/user_profile.json
  - Supports display and spoken naming

### Tooling Layer

- ToolManager:
  - Scans installed runtime tools/models
  - Downloads known assets and reports progress

## UI Binding

BackendFacade is the only QML bridge surface. It exposes:

- Reactive properties (state, transcript, models, settings)
- Runtime actions (start listening, refresh models, save settings)
- Setup actions (completeInitialSetup, runSetupScenario)

This keeps QML declarative and pushes behavior to C++.

## Logging and Observability

- startup.log: bootstrap and Qt log handler output
- jarvis.log: rotating structured app logs via spdlog
- logs/AI/*.log: one file per prompt/response exchange

## Test Coverage Snapshot

- ReasoningRouterTests: route selection behavior
- AiServicesTests: prompt, stream assembly, spoken reply sanitation
- IdentityProfileTests: identity/profile loading
- LocalResponseEngineTests: intent mapping and response quality guards
