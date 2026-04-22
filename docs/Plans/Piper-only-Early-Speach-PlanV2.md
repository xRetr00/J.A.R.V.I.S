# Piper-Only Early Speech Plan

## Summary

Introduce sentence-level early speech for Piper only by connecting existing stream sentence events to the existing Piper queue through a focused gating module. The current final response path remains the merge point for display, memory, logs, status, and fallback speech.

This plan explicitly uses:

- `StreamAssembler::sentenceReady` as the only streaming sentence trigger.
- Existing Piper queueing in `PiperTtsEngine::speakText` / `m_pendingTexts`.
- Existing `SpokenReply` parsing/sanitization to separate display text from speakable text.
- Existing `ResponseFinalizer` as the final merge point and fallback speech path.

Ownership boundary:

- Single ownership boundary: streaming partial-speech readiness, admission, commit tracking, and final speech suppression for safe Piper-only conversation turns.
- Standalone owner: new focused `StreamingSpeechGate` module.
- Why not `AssistantController.cpp`: controller should only wire stream events, TTS calls, state callbacks, and finalizer inputs; the gating rules are policy logic.
- How this avoids a new god object: the gate owns only early-speech readiness, buffering, segment budgeting, request/turn freshness, and finalizer-speech suppression. It does not own routing, memory, tools, narration, model calls, UI rendering, or TTS synthesis.

## 1. Target Behavior Model

Speech starts early only when all conditions are true:

- The active request is a normal streamed conversation request.
- `StreamAssembler::sentenceReady` emits a completed sentence from streamed deltas.
- Current TTS backend setting is Piper.
- The active response mode is `Chat`.
- No tool/action/confirmation/recovery route is active.
- The sentence passes the speech readiness policy: syntactic closure, semantic closure, non-filler quality, non-tool/pre-action nature, minimum value, and active-turn freshness.
- No prior cancellation/restart invalidated the request ID.

Speech must be delayed when:

- No completed sentence boundary has been emitted yet.
- The sentence ends with an ellipsis or continuation-style ending.
- The sentence is syntactically closed but semantically unfinished.
- The sentence feels like a lead-in to the next thought rather than a useful answer segment.
- The first sentence is only acknowledgement, filler, meta framing, or conversational padding.
- The sentence announces intent, planning, checking, or upcoming work instead of giving the answer.
- The streamed content appears to be structured output, JSON, code, hidden reasoning, status-only text, or tool-call related text.
- The request is not Piper-backed.
- The route is agent/tool/action/confirmation/recovery.
- The user interrupts, cancels, starts a new request, or restarts listening.

Sentence buffering behavior:

- Weak-but-possibly-useful sentences are buffered and merged with the next `sentenceReady` sentence.
- Filler-only or pre-action-only sentences are suppressed, not spoken and not carried forward unless they are attached to a useful answer sentence.
- If a buffered segment does not become speech-ready after two completed sentences, leave it for finalizer fallback instead of forcing early speech.
- `StreamAssembler::sentenceReady` remains the trigger, but `StreamingSpeechGate` decides whether that sentence is actually ready to speak.

Tool-call behavior:

- Early speech is fully suppressed for `AgentConversation`, `CommandExtraction`, deterministic task, background task, confirmation, recovery, and action-progress flows.
- If tools are involved, speech remains on the existing narration/finalizer path.
- Speech resumes only after tools complete and the final assistant response is available through the current finalization flow.
- The model must never speak speculative partial text before a tool result is known.

Speech resumes after early segment playback when:

- If the AI stream is still active, Vaxil returns to a stable `Processing`/thinking presentation and waits for more stream or final completion.
- If final completion arrives and early speech has actually committed, `ResponseFinalizer` finalizes display/memory/status but does not speak the full response again.
- If no early speech committed, `ResponseFinalizer` speaks exactly as it does today.

## 2. Minimal Architecture Change

Hook point:

- Add a connection to `StreamAssembler::sentenceReady` near the existing `partialTextUpdated` connection in [src/core/AssistantController.cpp](D:/Vaxil/src/core/AssistantController.cpp).
- The connection must delegate immediately to `StreamingSpeechGate`.
- `AssistantController.cpp` only passes request/session metadata and calls existing `m_ttsEngine->speakText(...)` when the gate returns an admitted segment.
- `AssistantController.cpp` must not contain the readiness rules, filler lists, pre-action rules, or merge/buffer policy.

Speech gating location:

- Add [src/core/StreamingSpeechGate.h](D:/Vaxil/src/core/StreamingSpeechGate.h) and [src/core/StreamingSpeechGate.cpp](D:/Vaxil/src/core/StreamingSpeechGate.cpp).
- The gate owns request freshness, Piper-only eligibility, sentence readiness, semantic/filler/pre-action suppression, length thresholds, segment budget, commit state, and finalizer speech-suppression state.
- It should be pure/core logic where practical, with no audio playback ownership.

Avoid touching Qwen:

- Do not modify [src/tts/QwenTtsEngine.cpp](D:/Vaxil/src/tts/QwenTtsEngine.cpp) or [src/tts/QwenTtsEngine.h](D:/Vaxil/src/tts/QwenTtsEngine.h).
- Do not change Qwen settings, Qwen download/setup, or Qwen fallback behavior.
- Early speech is a no-op unless `AppSettings::ttsEngineKind()` resolves to `piper`.

Avoid shared abstraction changes:

- Do not change [src/tts/TtsEngine.h](D:/Vaxil/src/tts/TtsEngine.h) in the MVP.
- Do not add a parallel TTS interface.
- Do not add a second audio path.
- Reuse `TtsEngine::speakText(...)`; Piper receives segments through the existing selectable/worker/runtime path only when Piper is selected.

Avoid duplicate finalizer output:

- `ResponseFinalizer` must still run for the final response.
- If early speech reaches the MVP commit latch, pass a copy of `SpokenReply` with `shouldSpeak=false` into finalization.
- Display text, memory append, prompt/response logging, status selection, and conversation-session handling remain centralized in `ResponseFinalizer`.

## 3. Piper-Only Integration Strategy

Sending partial segments into Piper:

- `StreamingSpeechGate` returns an admitted `spokenText` segment and `TtsUtteranceContext`.
- `AssistantController.cpp` calls the existing `m_ttsEngine->speakText(segment, context)`.
- Because the gate only admits when `ttsEngineKind == "piper"`, the call flows into the existing Piper queue.
- No direct `PiperTtsEngine` instance should be created or accessed from the controller.

Segment context tagging:

- Use existing `TtsUtteranceContext` fields.
- `utteranceClass`: `assistant_reply_stream_segment`
- `source`: `conversation_stream`
- `turnId`: stable request/session tag, for example `{requestId}:{actionSessionId}:stream:{segmentIndex}`
- `semanticTarget`: active session goal or user request

Piper queue tagging:

- Update only [src/tts/PiperTtsEngine.h](D:/Vaxil/src/tts/PiperTtsEngine.h) and [src/tts/PiperTtsEngine.cpp](D:/Vaxil/src/tts/PiperTtsEngine.cpp) to store a Piper-only queue item instead of only `QString`.
- Suggested internal item: text plus `TtsUtteranceContext`.
- Keep `QwenTtsEngine` queue unchanged.
- Keep `TtsEngine` interface unchanged.
- Existing Piper synthesis generation checks still drop stale in-flight synthesis results.

Commit acknowledgement:

- MVP should treat `m_ttsEngine->speakText(...)` returning from a Piper-eligible admitted segment as “submitted to TTS”, but not as “committed speech.”
- Add controller-to-gate notifications from existing `TtsEngine::playbackStarted`, `playbackFinished`, and `playbackFailed` callbacks so the gate can distinguish submitted, committed, finished, and failed states.
- Because `TtsEngine::speakText(...)` has no return value and the shared abstraction should not change in MVP, `playbackStarted` is the safest practical commit signal.

Outdated segment cancellation:

- `StreamingSpeechGate` stores the active AI request ID and increments/reset state on every new request.
- Stale `sentenceReady` emissions are ignored if request/session does not match.
- User cancel, speech interrupt, request restart, and new submit all invalidate the gate and call the existing `m_ttsEngine->clear()`.
- Piper `clear()` already clears pending text and invalidates active synthesis generation; keep that behavior.

## 4. Speech Gating Rules

### Speech Readiness Policy

A segment is admitted only if all readiness checks pass:

- Freshness: request ID, request kind, active session, and gate generation match the current turn.
- Backend: current TTS backend is Piper.
- Route: active request is streamed `Conversation` and active response mode is `Chat`.
- Syntactic closure: sentence boundary is strong enough to sound complete.
- Semantic closure: sentence gives useful answer content, not just setup.
- Non-filler quality: sentence is not acknowledgement, padding, or meta framing.
- Non-action nature: sentence does not announce intent, planning, checking, or upcoming tool-like work.
- Safety: sentence is not structured output, hidden reasoning, code, tool payload, status-only text, or speculative tool-adjacent text.
- Budget: early speech remains within the current spoken summary budget.

First safe sentence rule:

- Candidate must come from `StreamAssembler::sentenceReady`.
- Candidate must sanitize into non-empty `SpokenReply::spokenText`.
- `SpokenReply::shouldSpeak` must be true.
- Candidate must not look like JSON/structured output.
- Candidate must not contain `display_text`, `spoken_text`, `should_speak`, `tool_calls`, `background_tasks`, code fences, or hidden reasoning markers.
- Candidate must not be status-only text.
- Candidate must pass stricter first-sentence readiness than later segments.

Sentence fragmentation and closure rules:

- Delay if the candidate ends with `...`, a trailing colon, an open parenthetical, an unclosed quote, or a connector-like ending.
- Delay if the candidate starts an enumeration or setup pattern and appears to require the next sentence to make sense.
- Delay if the candidate ends with phrases such as `for example`, `such as`, `because`, `which means`, `so`, `but`, `and`, `or`, `the key point is`, `here is`, or `there are`.
- Delay if the candidate is only a lead-in, such as `There are a few things to consider.` or `The short answer is this.`
- Merge delayed candidates with the next completed sentence and re-run readiness on the merged text.
- Suppress rather than merge if the candidate is purely filler or purely pre-action text.
- Do not modify `StreamAssembler` boundary detection in MVP; all closure decisions live in `StreamingSpeechGate`.

First-sentence trap and filler opening rules:

- First sentence rules are stricter than later sentence rules.
- Suppress filler-only first sentences like `Okay.`, `Sure.`, `Got it.`, `Understood.`, `Absolutely.`, `No problem.`, and `I can help with that.`
- Suppress or merge meta openings like `Let me explain.`, `Here is the answer.`, `The short answer is...`, `There are a few parts to this.`, and `I will walk through it.`
- Detect filler by normalized lowercase/canonical text after stripping punctuation and extra whitespace.
- Allow a first sentence only when it contains answer-bearing content, such as a direct fact, recommendation, conclusion, explanation, or user-facing result.
- If a filler opening is followed by a useful second sentence, drop the filler and admit only the useful sentence or merged useful text.

Semantic commitment and pre-action filtering:

- Block sentences that announce future work rather than answer content.
- Block pre-action text like `I will check that now.`, `I will look into this.`, `Let me check.`, `Let me analyze this.`, `First, I need to analyze...`, and `I am going to verify...`
- Block tool-adjacent speculative text like `I will open the browser`, `I need to search`, `I will inspect the logs`, `I will run a command`, or `I will check the file`.
- Block planning sentences that describe process rather than outcome.
- For MVP, implement this with a small explicit denylist of normalized phrase families inside `StreamingSpeechGate`, not prompt changes and not controller logic.
- If pre-action text appears in normal `Conversation`, do not speak it early; let finalizer fallback decide after full response completion.
- If a sentence contains both pre-action framing and useful answer content, strip or delay only if safe stripping is simple; otherwise delay to the next sentence or leave to finalizer.

Minimum length:

- First early segment: at least 60 sanitized characters or at least 10 words after filler/pre-action stripping.
- Later early segments: at least 32 sanitized characters or at least 6 words.
- Short completed sentences are buffered with the next completed sentence only if they are not filler-only and not pre-action-only.
- If buffered text still does not pass readiness after two sentences, do not early-speak it; leave it to finalizer fallback.

Punctuation trigger:

- Only `.`, `!`, and `?` sentence boundaries from `StreamAssembler::sentenceReady` can trigger early speech.
- Treat `...` as weak closure and delay in MVP.
- Do not trigger on commas, semicolons, colons, newlines, or raw partial deltas.
- Do not modify `StreamAssembler` boundary behavior in MVP.

Long-sentence handling:

- MVP accepts long first-sentence latency rather than forcing unsafe clause splitting.
- Do not split on commas, semicolons, em dashes, or conjunctions in MVP because semantic completeness is unreliable and bad splits sound worse than waiting.
- If a single first sentence exceeds about 280 characters before any early speech, allow the gate to admit it only if it ends with strong punctuation and passes all readiness checks.
- If a single sentence exceeds about 420 characters, leave it to finalizer fallback unless a future dedicated clause-splitting policy is added.
- Future version can add conservative clause splitting at strong discourse boundaries, but only after tests and only inside `StreamingSpeechGate`.

Cumulative spoken budget:

- Keep early speech within the current spoken-summary budget: maximum 3 spoken sentences or about 280 characters total.
- Once the budget is reached, stop submitting further stream segments.
- If more display text continues after the spoken budget, the full response remains on screen through existing display/finalizer behavior.

Tool-call suppression:

- Suppress early speech when `activeRequestKind != Conversation`.
- Suppress when response mode is not `Chat`.
- Suppress when the active action session has selected tools, pending confirmation, tool progress, recovery mode, or action mode.
- Suppress when any agent/tool loop is active.
- Suppress if the sentence references using tools or checking external/local state before a tool result is available.

Stability window:

- MVP: no timer-based stability window; completed sentence boundary plus readiness checks are the stability signal.
- Future optional version: add a 120-200 ms coalescing window before submitting the first segment, but only inside `StreamingSpeechGate`.

## 5. Finalizer Interaction

Finalizer still speaks when:

- Piper early speech is disabled by backend selection.
- Streaming is disabled.
- No sentence passed the gate.
- A segment was admitted/submitted but playback never started and the request reaches finalization.
- The request is tool/action/agent/confirmation/recovery.
- The stream ends before any safe segment is committed.
- The gate was canceled before any committed playback.

Finalizer speech is suppressed when:

- At least one early segment reached `TtsEngine::playbackStarted` for the active request and gate generation.
- Suppression affects only `SpokenReply::shouldSpeak`; it does not skip finalization.

Commit latch rule:

- MVP commit latch is `playbackStarted`, not gate admit and not `speakText` call.
- Gate admit only means “ready to submit.”
- `speakText` call only means “submitted to current TTS path.”
- `playbackStarted` means audio actually began and the user likely heard speech.
- This is the safest practical MVP latch because the shared `TtsEngine::speakText(...)` has no success return and shared abstractions should not change.

Segment submitted but Piper never starts:

- Gate state remains `submitted_not_committed`.
- Finalizer speech is not suppressed.
- If final response arrives before playback starts, finalizer can still speak the final response.
- If Piper later starts stale submitted audio after finalization, request/generation checks must cause the gate/controller path to ignore or clear it.
- If playback fails before `playbackStarted`, finalizer fallback remains available.

Double-speech prevention:

- `StreamingSpeechGate` exposes distinct state such as `hasSubmittedEarlySpeech()` and `hasCommittedEarlySpeech()`.
- `finalizeReply(...)` suppresses speech only when `hasCommittedEarlySpeech() == true`.
- If committed early speech exists, the returned reply preserves `displayText` and `spokenText` but sets `shouldSpeak=false`.
- `ResponseFinalizer` remains the single final display/memory/status/log merge point.

Fallback rule:

- If `hasCommittedEarlySpeech() == false`, `finalizeReply(...)` passes the original `SpokenReply` unchanged.
- This preserves the current finalization speech behavior and avoids silent failure.

## 6. State Model Update

MVP should not add a new public `Responding` state.

Use existing states:

- `Processing`: model is generating or waiting for final response.
- `Speaking`: audio is currently playing.
- `Idle/Open`: final response is done and no speech is active.

UX jitter risk:

- Early speech can cause `Processing -> Speaking -> Processing -> Speaking` oscillation while the stream continues.
- MVP should avoid adding a public state, but should add an internal flag such as `streamingSpeechActive` or derive equivalent state from `StreamingSpeechGate`.
- While `streamingSpeechActive` and the AI request is still active, `playbackFinished` must not restart wake/listening and must not mark the turn idle.
- Status text should remain stable, such as `Responding...` or `Generating response...`, instead of rapidly flipping between `Speaking` and `Processing request`.
- Existing orb/surface can still receive `speakingRequested` for audio animation, but high-level lifecycle should remain the active response turn.

Required internal behavior:

- When early speech starts, existing `playbackStarted` can still emit `speakingRequested`.
- When early speech finishes but the AI request is still active, do not resume wake/listening and do not mark the turn idle.
- Instead, return to a stable processing/responding presentation and keep waiting for final completion.
- When final completion arrives and finalizer speech is suppressed, use the existing no-speech finalization branch to reopen the conversation session or wake monitor.

Advanced version:

- Add a distinct UI-facing `Responding` or `StreamingResponse` state only if the current `Processing`/`Speaking` presentation still feels visually wrong after MVP.
- Add smoother UI labeling and animation states after behavior correctness is proven.
- Do not add public state in MVP because it expands QML/view-model state surface area and increases regression risk.

## 7. Cancellation & Safety

Cancel queued segments:

- Existing `m_ttsEngine->clear()` must remain the only queue-clearing mechanism from the controller.
- Piper `clear()` clears `m_pendingTexts`, resets processing state, invalidates generation, and stops playback.
- Do not add a second queue in the controller.

User interrupt:

- On `interruptSpeechAndListen()`, invalidate `StreamingSpeechGate`, reset `StreamAssembler`, cancel the active AI request, and clear TTS if early speech was submitted, committed, or TTS is speaking.
- If early speech was submitted but not committed, finalizer fallback should not run for the canceled request.
- Preserve conversation-session behavior after interruption.

Request restart:

- On new `submitText(...)`, reset `StreamAssembler`, clear TTS, and begin a fresh streaming-speech gate turn.
- Old `sentenceReady` emissions must be ignored by request ID and gate generation.
- Submitted-but-not-committed old segments must be cleared before the new request is allowed to speak.

Stop speaking:

- On `stopSpeaking()`, invalidate early speech for the current request and clear TTS.
- Do not let the finalizer later speak the same response after the user explicitly stopped speech.
- Treat user stop as stronger than “no committed early speech”; it cancels final speech for that request.

Backend/request failure:

- If the AI request fails before any segment is committed, existing error/fallback behavior remains.
- If Piper fails after submit but before playback starts, finalizer fallback remains allowed if the AI request completes normally.
- If Piper fails after committed playback, use existing `playbackFailed` behavior; do not retry through Qwen and do not speak the full final response again.
- Future improvement can add explicit pre-audio retry/fallback policy, but MVP should not add cross-backend retry complexity.

Silent failure protection:

- Log submitted, committed, failed-before-commit, failed-after-commit, and finalizer-suppressed decisions.
- Suppress finalizer speech only for committed audio, not merely admitted/submitted text.
- If no commit occurred, finalizer remains the fallback.

## 8. Step-by-Step Implementation Plan

### Step 1: Add Characterization Tests First

Files to modify:

- [tests/AiServicesTests.cpp](D:/Vaxil/tests/AiServicesTests.cpp) or a new focused test file under [tests](D:/Vaxil/tests)

Purpose:

- Lock existing `StreamAssembler::sentenceReady` behavior.
- Lock existing `SpokenReply` sanitation/status-only/truncation behavior.
- Add tests describing desired streaming-speech gate decisions before wiring runtime behavior.
- Add specific cases for ellipsis, weak closure, filler openings, pre-action sentences, long sentences, and committed-vs-submitted finalizer fallback.

Risk level: low

### Step 2: Add `StreamingSpeechGate`

Files to modify:

- [src/core/StreamingSpeechGate.h](D:/Vaxil/src/core/StreamingSpeechGate.h)
- [src/core/StreamingSpeechGate.cpp](D:/Vaxil/src/core/StreamingSpeechGate.cpp)
- Build config file that registers new source files, likely [CMakeLists.txt](D:/Vaxil/CMakeLists.txt)

Purpose:

- Own Piper-only early-speech eligibility.
- Track active request ID, action session ID, segment index, cumulative spoken budget, buffered sentences, submitted state, committed state, failure state, and cancellation state.
- Return explicit decisions: admit, delay_buffer, suppress, stale, budget_exhausted, not_ready, pre_action, filler, weak_closure.
- Implement the lightweight readiness checklist in this module, not in the controller.

Risk level: low

### Step 3: Add Gate Unit Tests

Files to modify:

- [tests/AiServicesTests.cpp](D:/Vaxil/tests/AiServicesTests.cpp) or new `StreamingSpeechGateTests.cpp`
- [CMakeLists.txt](D:/Vaxil/CMakeLists.txt) if a new test target is created

Purpose:

- Verify Piper admits normal streamed conversation sentences.
- Verify Qwen suppresses early speech.
- Verify tool/action/confirmation/recovery suppresses early speech.
- Verify short first sentence buffers instead of speaking.
- Verify filler first sentences are suppressed or dropped before useful content.
- Verify pre-action/planning/tool-adjacent sentences are blocked.
- Verify ellipsis and weak closure are delayed and merge with the next sentence.
- Verify long first sentences are not forcibly split in MVP.
- Verify JSON/structured/code/reasoning/status-only text is suppressed.
- Verify cumulative budget stops after current spoken-summary limits.
- Verify finalizer suppression latch is set only after playback-start commit, not after admit or submit.

Risk level: low

### Step 4: Wire Turn Lifecycle in Controller

Files to modify:

- [src/core/AssistantController.h](D:/Vaxil/src/core/AssistantController.h)
- [src/core/AssistantController.cpp](D:/Vaxil/src/core/AssistantController.cpp)

Purpose:

- Add `std::unique_ptr<StreamingSpeechGate>`.
- On new user submit, reset/begin gate state.
- On AI request start, bind the active request ID to the gate.
- On cancellation/restart/interrupt/stop-speaking, invalidate the gate.
- Keep edits limited to wiring/delegation.

Risk level: medium

### Step 5: Hook `sentenceReady` to Existing TTS Path

Files to modify:

- [src/core/AssistantController.cpp](D:/Vaxil/src/core/AssistantController.cpp)

Purpose:

- Connect `StreamAssembler::sentenceReady` to a small lambda.
- Lambda delegates sentence and current metadata to `StreamingSpeechGate`.
- If admitted, call existing `m_ttsEngine->speakText(admittedText, context)` and notify the gate that the segment was submitted.
- Do not alter `partialTextUpdated`; screen text should continue to update as it does today.
- Do not call Qwen-specific code.
- Do not create direct Piper instances.

Risk level: medium

### Step 6: Track Playback Commit and Prevent Premature Idle

Files to modify:

- [src/core/AssistantController.cpp](D:/Vaxil/src/core/AssistantController.cpp)

Purpose:

- Notify `StreamingSpeechGate` from existing `TtsEngine::playbackStarted`, `playbackFinished`, and `playbackFailed` handlers.
- Treat `playbackStarted` as the MVP early-speech commit latch.
- If early speech finishes while the AI request is still active, return to stable `Processing`/responding behavior instead of ending the session or restarting wake/listening.
- Existing playback-finished behavior remains unchanged for normal finalizer speech.

Risk level: medium/high because it touches wake/listen lifecycle timing

### Step 7: Suppress Finalizer Speech Only After Committed Early Speech

Files to modify:

- [src/core/AssistantController.cpp](D:/Vaxil/src/core/AssistantController.cpp)

Purpose:

- In `finalizeReply(...)`, ask `StreamingSpeechGate` whether final speech should be suppressed.
- Pass a copied `SpokenReply` with `shouldSpeak=false` only when early speech committed via playback start.
- If a segment was only admitted or submitted but never started playback, allow finalizer fallback.
- Keep `ResponseFinalizer` unchanged in MVP.
- Keep display, memory, prompt logs, and status centralized in `ResponseFinalizer`.

Risk level: medium

### Step 8: Add Piper-Only Queue Metadata

Files to modify:

- [src/tts/PiperTtsEngine.h](D:/Vaxil/src/tts/PiperTtsEngine.h)
- [src/tts/PiperTtsEngine.cpp](D:/Vaxil/src/tts/PiperTtsEngine.cpp)

Purpose:

- Replace Piper’s internal pending text queue with a Piper-only queue item containing prepared text and `TtsUtteranceContext`.
- Log segment source, utterance class, turn ID, and queue position during enqueue/process.
- Preserve current synthesis, generation invalidation, clear behavior, playback, and far-end frame behavior.
- Do not make equivalent changes in Qwen.

Risk level: low/medium

### Step 9: Add Focused TTS/State Logging

Files to modify:

- [src/core/StreamingSpeechGate.cpp](D:/Vaxil/src/core/StreamingSpeechGate.cpp)
- [src/core/AssistantController.cpp](D:/Vaxil/src/core/AssistantController.cpp)
- [src/tts/PiperTtsEngine.cpp](D:/Vaxil/src/tts/PiperTtsEngine.cpp)

Purpose:

- Add small targeted logs only:
  - `[stream_speech_gate] decision=... reason=... requestId=...`
  - `[stream_speech_submit] segment=... chars=... turn=...`
  - `[stream_speech_commit] playbackStarted=true requestId=... segment=...`
  - `[stream_speech_failed] committed=... reason=...`
  - `[stream_speech_finalizer] suppressSpeak=... reason=...`
- Avoid broad startup or noisy per-token logs.

Risk level: low

### Step 10: Verify

Files to modify:

- None unless tests expose a defect

Purpose:

- Build and run targeted tests:
  - `jarvis_ai_services_tests`
  - `jarvis_response_finalizer_tests`
  - any new `streaming_speech_gate` test target
- Run broader relevant CTest pattern if available:
  - `ctest --test-dir build-release -R "jarvis_(ai_services|response_finalizer|tts_speech_preparation)_tests" --output-on-failure`
- Manual smoke:
  - Piper selected, streamed long chat response starts speaking after first safe answer-bearing sentence.
  - First streamed sentence `Okay.` or `Let me explain.` does not speak by itself.
  - Ellipsis ending delays and merges with the next sentence.
  - Pre-action sentence does not speak early.
  - Qwen selected, no early speech occurs; finalizer behavior remains unchanged.
  - Tool request, no early speech before tool completion.
  - Segment submitted but no playback start still allows finalizer fallback.
  - Segment with playback start suppresses finalizer speech.
  - Interrupt during early speech cancels queued audio and starts listening.
  - New request while old response streams does not speak stale segments.

Risk level: low

## 9. Anti-Regression Rules

Must not change:

- Qwen TTS behavior.
- Qwen fallback behavior in `SelectableTtsEngine`.
- `QwenTtsEngine` files.
- `TtsEngine` public interface.
- AI request routing categories.
- Tool execution flow.
- Agent/tool-call flow.
- Response display updates from `StreamAssembler::partialTextUpdated`.
- Final display/memory/status/log behavior in `ResponseFinalizer`.
- Existing finalizer speech behavior when early speech does not commit.
- Existing `SpokenReply` sanitation and spoken length protection.
- Existing Piper synthesis/playback pipeline except for Piper-only queue metadata/logging.
- Wake/listen restart behavior for non-streaming finalizer speech.

Must remain identical:

- If Piper early speech is not admitted, the user hears exactly the same final response path as before.
- If Piper early speech is submitted but never reaches playback start, finalizer fallback remains available.
- If Qwen is selected, early speech is completely inactive.
- If a request uses tools, speech waits for the existing narration/final response.
- If the user cancels, queued speech is cleared and stale stream chunks are ignored.
- If streaming is disabled, no early speech is attempted.

## 10. MVP vs Advanced Version

MVP:

- Piper-only.
- Conversation-only.
- Sentence-level, not token-level.
- Uses `StreamAssembler::sentenceReady`.
- Uses existing `m_ttsEngine->speakText(...)` path.
- Uses existing Piper queue.
- Adds focused `StreamingSpeechGate`.
- Adds readiness checks for syntactic closure, semantic closure, filler suppression, pre-action suppression, and turn freshness.
- Buffers weak-but-useful sentence fragments and merges with the next sentence.
- Suppresses filler-only and pre-action-only first sentences.
- Accepts long-sentence latency rather than forcing clause splitting.
- Uses `playbackStarted` as the early-speech commit latch.
- Suppresses finalizer speech only after committed playback, not after admit or submit.
- Keeps `ResponseFinalizer` as final merge/fallback.
- Does not add a public `Responding` state.
- Uses a small internal streaming-speech flag to avoid lifecycle jitter.
- Does not touch Qwen.
- Does not change shared TTS abstractions.

Future improvements:

- Add a UI-facing `Responding` or `StreamingResponse` state if the current `Processing`/`Speaking` presentation is still unstable.
- Add a short first-segment stability/coalescing timer.
- Add conservative clause splitting for very long sentences after tests prove safe boundaries.
- Add true in-flight Piper process cancellation instead of only stale-result dropping.
- Add explicit finalizer retry/fallback policy for pre-audio Piper failures.
- Add smarter semantic gating for partial answers that depend on retrieved evidence.
- Add per-segment audio continuity tuning.
- Add streaming-aware tool narration only after a separate tool-flow design, not in this MVP.

## What Changed In This Revision

- Added sentence-fragmentation protection so `sentenceReady` is treated as a candidate boundary, not automatic speech permission.
- Added stricter first-sentence rules to avoid speaking filler, acknowledgements, and low-value openings.
- Added semantic commitment filtering to block pre-action, planning, and tool-adjacent speculative speech.
- Changed the finalizer suppression latch from “submitted” to `playbackStarted` so silent Piper failures still fall back to finalizer speech.
- Added explicit long-sentence policy: do not force clause splitting in MVP; accept latency unless a full sentence is safely ready.
- Added UX jitter handling with internal streaming-speech state while avoiding a new public `Responding` state.
- Added a lightweight speech readiness checklist combining syntactic closure, semantic closure, non-filler quality, non-action nature, and turn freshness.
